#include "gura/codegen/LLVMCodeGen.h"

#include "gura/ast/Expr.h"

#if GURA_HAS_LLVM
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>
#endif

#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gura {

#if GURA_HAS_LLVM
namespace {

bool isCoreBuiltinName(std::string_view name) {
  return name == "print" || name == "println" || name == "print_i64" || name == "println_i64" || name == "readln_i64";
}

std::string joinPath(const std::vector<std::string>& segments) {
  std::string result;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (i != 0) {
      result += ".";
    }
    result += segments[i];
  }
  return result;
}

std::string qualifiedName(const ast::Path& modulePath, std::string_view name) {
  std::vector<std::string> segments = modulePath.segments;
  segments.emplace_back(name);
  return joinPath(segments);
}

std::string manglePath(std::string_view key) {
  if (key == "main" || key.find('.') == std::string_view::npos) {
    return std::string(key);
  }
  if (key.ends_with(".main")) {
    return "main";
  }
  std::string result = "gura_";
  for (const char ch : key) {
    result += ch == '.' ? '_' : ch;
  }
  return result;
}

ast::Path pathFromFieldAccess(const ast::Expr& expr) {
  if (const auto* name = dynamic_cast<const ast::NameExpr*>(&expr)) {
    return ast::Path{{name->name}};
  }
  if (const auto* field = dynamic_cast<const ast::FieldAccessExpr*>(&expr)) {
    ast::Path path = pathFromFieldAccess(*field->object);
    if (!path.segments.empty()) {
      path.segments.push_back(field->fieldName);
    }
    return path;
  }
  return {};
}

std::optional<std::string> qualifiedCoreBuiltinName(const ast::Expr& expr) {
  const auto* leaf = dynamic_cast<const ast::FieldAccessExpr*>(&expr);
  if (leaf == nullptr || !isCoreBuiltinName(leaf->fieldName)) {
    return std::nullopt;
  }
  const auto* core = dynamic_cast<const ast::FieldAccessExpr*>(leaf->object.get());
  if (core == nullptr || core->fieldName != "core") {
    return std::nullopt;
  }
  const auto* stdName = dynamic_cast<const ast::NameExpr*>(core->object.get());
  if (stdName == nullptr || stdName->name != "std") {
    return std::nullopt;
  }
  return leaf->fieldName;
}

class ModuleEmitter {
  struct FieldLayout {
    std::size_t index = 0;
    llvm::Type* type = nullptr;
  };

  struct MethodLayout {
    std::string structName;
    const ast::FnDecl* decl = nullptr;
    llvm::Function* function = nullptr;
  };

  struct StructLayout {
    llvm::StructType* type = nullptr;
    std::uint64_t typeId = 0;
    std::vector<std::string> fieldNames;
    std::unordered_map<std::string, FieldLayout> fields;
    std::unordered_map<std::string, MethodLayout> methods;
    std::unordered_map<std::string, MethodLayout> associatedFunctions;
  };

public:
  ModuleEmitter() : module_("gura", context_), builder_(context_) {}

  std::string emit(const ast::SourceFile& file) {
    collectStructs(file);
    collectMethods(file);
    for (const auto& decl : file.declarations) {
      if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
        if (fn->genericParams.empty()) {
          registerFunctionKey(*fn);
        }
      }
    }
    for (const auto& decl : file.declarations) {
      if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
        if (fn->genericParams.empty()) {
          declareFunction(*fn);
        }
      }
    }
    declareMethods();
    for (const auto& decl : file.declarations) {
      if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
        if (fn->genericParams.empty()) {
          emitFunction(*fn);
        }
      }
    }
    emitMethods();
    std::string output;
    llvm::raw_string_ostream stream(output);
    module_.print(stream, nullptr);
    return output;
  }

private:
  void collectStructs(const ast::SourceFile& file) {
    std::uint64_t nextTypeId = 1;
    for (const auto& decl : file.declarations) {
      const auto* structDecl = dynamic_cast<const ast::StructDecl*>(decl.get());
      if (structDecl == nullptr) {
        continue;
      }
      StructLayout layout;
      layout.type = llvm::StructType::create(context_, structDecl->name);
      layout.typeId = nextTypeId++;
      layouts_.emplace(structDecl->name, std::move(layout));
    }
    for (const auto& decl : file.declarations) {
      const auto* structDecl = dynamic_cast<const ast::StructDecl*>(decl.get());
      if (structDecl == nullptr) {
        continue;
      }
      auto& layout = layouts_.at(structDecl->name);
      std::vector<llvm::Type*> fieldTypes;
      fieldTypes.reserve(structDecl->fields.size());
      for (std::size_t i = 0; i < structDecl->fields.size(); ++i) {
        const auto& field = structDecl->fields[i];
        llvm::Type* fieldType = typeFromAst(field.type.get());
        fieldTypes.push_back(fieldType);
        layout.fieldNames.push_back(field.name);
        layout.fields.emplace(field.name, FieldLayout{i, fieldType});
      }
      layout.type->setBody(fieldTypes, false);
    }
  }

  void collectMethods(const ast::SourceFile& file) {
    for (const auto& decl : file.declarations) {
      if (const auto* structDecl = dynamic_cast<const ast::StructDecl*>(decl.get())) {
        for (const auto& method : structDecl->methods) {
          if (method != nullptr) {
            collectMethod(structDecl->name, *method);
          }
        }
      }
      if (const auto* implDecl = dynamic_cast<const ast::ImplDecl*>(decl.get())) {
        if (implDecl->traitName.has_value()) {
          continue;
        }
        for (const auto& method : implDecl->methods) {
          if (method != nullptr) {
            collectMethod(implDecl->typeName, *method);
          }
        }
      }
    }
  }

  void collectMethod(const std::string& structName, const ast::FnDecl& method) {
    if (!layouts_.contains(structName)) {
      return;
    }
    if (!method.params.empty() && method.params.front().name == "self") {
      layouts_.at(structName).methods.emplace(method.name, MethodLayout{structName, &method, nullptr});
      return;
    }
    layouts_.at(structName).associatedFunctions.emplace(method.name, MethodLayout{structName, &method, nullptr});
  }

  std::string mangleMethodName(std::string_view structName, std::string_view methodName) const {
    return std::string(structName) + "_" + std::string(methodName);
  }

  llvm::Type* typeFromAst(const ast::TypeRef* type) {
    if (type == nullptr || type->name == "unit") {
      return llvm::Type::getVoidTy(context_);
    }
    if (type->isArray) {
      return llvm::ArrayType::get(typeFromAst(type->elementType.get()), 0);
    }
    if (type->capability == ast::Capability::Iso) {
      return llvm::PointerType::getUnqual(context_);
    }
    if (type->name == "bool") {
      return llvm::Type::getInt1Ty(context_);
    }
    if (type->name == "i32") {
      return llvm::Type::getInt32Ty(context_);
    }
    if (type->name == "f32") {
      return llvm::Type::getFloatTy(context_);
    }
    if (type->name == "f64") {
      return llvm::Type::getDoubleTy(context_);
    }
    if (layouts_.contains(type->name)) {
      return layouts_.at(type->name).type;
    }
    return llvm::Type::getInt64Ty(context_);
  }

  std::string decodeStringLiteral(std::string_view text) const {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
      text.remove_prefix(1);
      text.remove_suffix(1);
    }
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
      if (text[i] != '\\' || i + 1 >= text.size()) {
        result.push_back(text[i]);
        continue;
      }
      const char escaped = text[++i];
      switch (escaped) {
      case 'n': result.push_back('\n'); break;
      case 't': result.push_back('\t'); break;
      case '"': result.push_back('"'); break;
      case '\\': result.push_back('\\'); break;
      case '0': result.push_back('\0'); break;
      default: result.push_back(escaped); break;
      }
    }
    return result;
  }

  llvm::Type* parameterTypeFromAst(const ast::TypeRef* type) {
    if (type != nullptr && type->isArray) {
      return llvm::PointerType::getUnqual(context_);
    }
    return typeFromAst(type);
  }

  std::uint32_t regionStrategyValue(std::string_view strategy) const {
    if (strategy.empty() || strategy == "Arena") {
      return 0;
    }
    if (strategy == "RC") {
      return 1;
    }
    if (strategy == "GC") {
      return 2;
    }
    if (strategy == "Manual") {
      return 3;
    }
    return 0;
  }

  llvm::FunctionCallee regionNewIso() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(ptrTy, {llvm::Type::getInt64Ty(context_), llvm::Type::getInt32Ty(context_)}, false);
    return module_.getOrInsertFunction("__gura_region_new_iso", fnTy);
  }

  llvm::FunctionCallee regionEnter() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_enter", fnTy);
  }

  llvm::FunctionCallee regionExit() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {ptrTy, ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_exit", fnTy);
  }

  llvm::FunctionCallee regionExplore() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_explore", fnTy);
  }

  llvm::FunctionCallee regionExploreExit() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_explore_exit", fnTy);
  }

  llvm::FunctionCallee regionBridge() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_bridge", fnTy);
  }

  llvm::FunctionCallee regionSetBridgeType() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {ptrTy, llvm::Type::getInt64Ty(context_)}, false);
    return module_.getOrInsertFunction("__gura_region_set_bridge_type", fnTy);
  }

  llvm::FunctionCallee regionFreeze() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(ptrTy, {ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_freeze", fnTy);
  }

  llvm::FunctionCallee regionMerge() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false);
    return module_.getOrInsertFunction("__gura_region_merge", fnTy);
  }

  llvm::FunctionCallee putsFunction() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), {ptrTy}, false);
    return module_.getOrInsertFunction("puts", fnTy);
  }

  llvm::FunctionCallee printfFunction() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), {ptrTy}, true);
    return module_.getOrInsertFunction("printf", fnTy);
  }

  llvm::FunctionCallee scanfFunction() {
    auto* ptrTy = llvm::PointerType::getUnqual(context_);
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), {ptrTy}, true);
    return module_.getOrInsertFunction("scanf", fnTy);
  }

  std::string functionKey(const ast::FnDecl& fn) const {
    return fn.modulePath.segments.empty() ? fn.name : qualifiedName(fn.modulePath, fn.name);
  }

  void registerFunctionKey(const ast::FnDecl& fn) {
    const std::string key = functionKey(fn);
    simpleFunctionKeys_[fn.name].push_back(key);
  }

  std::optional<std::string> resolveFunctionKey(const ast::Path& path) const {
    if (path.segments.empty()) {
      return std::nullopt;
    }
    if (path.segments.size() == 1) {
      const auto it = simpleFunctionKeys_.find(path.segments.front());
      if (it != simpleFunctionKeys_.end() && it->second.size() == 1) {
        return it->second.front();
      }
      return std::nullopt;
    }
    if (currentDecl_ != nullptr) {
      for (const auto& import : currentDecl_->imports) {
        if (!import.alias.has_value() || *import.alias != path.segments.front()) {
          continue;
        }
        ast::Path resolved = import.path;
        resolved.segments.insert(resolved.segments.end(), path.segments.begin() + 1, path.segments.end());
        const std::string key = joinPath(resolved.segments);
        if (functions_.contains(key)) {
          return key;
        }
      }
    }
    const std::string key = joinPath(path.segments);
    if (functions_.contains(key)) {
      return key;
    }
    return std::nullopt;
  }

  void declareFunction(const ast::FnDecl& fn) {
    std::vector<llvm::Type*> params;
    params.reserve(fn.params.size());
    for (const auto& param : fn.params) {
      params.push_back(parameterTypeFromAst(param.type.get()));
    }
    const std::string key = functionKey(fn);
    auto* fnType = llvm::FunctionType::get(typeFromAst(fn.returnType.get()), params, false);
    auto* function = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, manglePath(key), module_);
    std::size_t index = 0;
    for (auto& arg : function->args()) {
      arg.setName(fn.params[index++].name);
    }
    functions_.emplace(key, function);
  }

  void declareMethods() {
    for (auto& [structName, layout] : layouts_) {
      for (auto& [methodName, method] : layout.methods) {
        declareMethod(structName, methodName, method, true);
      }
      for (auto& [functionName, function] : layout.associatedFunctions) {
        declareMethod(structName, functionName, function, false);
      }
    }
  }

  void declareMethod(std::string_view structName, std::string_view methodName, MethodLayout& method, bool hasReceiver) {
    std::vector<llvm::Type*> params;
    params.reserve(method.decl->params.size());
    if (hasReceiver) {
      params.push_back(llvm::PointerType::getUnqual(layouts_.at(std::string(structName)).type));
    }
    const std::size_t firstParam = hasReceiver ? 1 : 0;
    for (std::size_t i = firstParam; i < method.decl->params.size(); ++i) {
      params.push_back(parameterTypeFromAst(method.decl->params[i].type.get()));
    }
    auto* fnType = llvm::FunctionType::get(typeFromAst(method.decl->returnType.get()), params, false);
    method.function = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, mangleMethodName(structName, methodName), module_);
    std::size_t index = 0;
    for (auto& arg : method.function->args()) {
      arg.setName(method.decl->params[index++].name);
    }
  }

  void emitFunction(const ast::FnDecl& fn) {
    const ast::Decl* previousDecl = currentDecl_;
    currentDecl_ = &fn;
    llvm::Function* function = functions_.at(functionKey(fn));
    auto* entry = llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);
    values_.clear();
    pointerValues_.clear();
    arrayPointers_.clear();
    pointerStructTypes_.clear();
    regionStructTypes_.clear();
    bridgeSlots_.clear();
    activeRegions_.clear();
    std::size_t paramIndex = 0;
    for (auto& arg : function->args()) {
      const auto& param = fn.params[paramIndex++];
      if (param.type != nullptr && param.type->isArray) {
        arrayPointers_.emplace(param.name, &arg);
        continue;
      }
      auto* slot = createEntryAlloca(function, arg.getName().str(), arg.getType());
      builder_.CreateStore(&arg, slot);
      values_.emplace(std::string(arg.getName()), slot);
    }
    if (fn.body != nullptr) {
      emitBlock(*fn.body);
    }
    emitDefaultReturn(function);
    currentDecl_ = previousDecl;
  }

  void emitMethods() {
    for (auto& [_, layout] : layouts_) {
      for (auto& [__, method] : layout.methods) {
        emitMethod(method, true);
      }
      for (auto& [__, function] : layout.associatedFunctions) {
        emitMethod(function, false);
      }
    }
  }

  void emitMethod(const MethodLayout& method, bool hasReceiver) {
    llvm::Function* function = method.function;
    auto* entry = llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);
    values_.clear();
    pointerValues_.clear();
    arrayPointers_.clear();
    pointerStructTypes_.clear();
    regionStructTypes_.clear();
    bridgeSlots_.clear();
    activeRegions_.clear();
    auto argIt = function->arg_begin();
    std::size_t firstParam = 0;
    if (hasReceiver) {
      pointerValues_.emplace(method.decl->params.front().name, &*argIt);
      pointerStructTypes_.emplace(method.decl->params.front().name, layouts_.at(method.structName).type);
      ++argIt;
      firstParam = 1;
    }
    for (std::size_t i = firstParam; i < method.decl->params.size(); ++i, ++argIt) {
      const auto& param = method.decl->params[i];
      if (param.type != nullptr && param.type->isArray) {
        arrayPointers_.emplace(param.name, &*argIt);
        continue;
      }
      auto* slot = createEntryAlloca(function, argIt->getName().str(), argIt->getType());
      builder_.CreateStore(&*argIt, slot);
      values_.emplace(std::string(argIt->getName()), slot);
    }
    if (method.decl->body != nullptr) {
      emitBlock(*method.decl->body);
    }
    emitDefaultReturn(function);
  }

  void emitDefaultReturn(llvm::Function* function) {
    if (!builder_.GetInsertBlock()->getTerminator()) {
      if (function->getReturnType()->isVoidTy()) {
        builder_.CreateRetVoid();
      } else {
        builder_.CreateRet(llvm::Constant::getNullValue(function->getReturnType()));
      }
    }
  }

  llvm::AllocaInst* createEntryAlloca(llvm::Function* function, std::string_view name, llvm::Type* type) {
    llvm::IRBuilder<> entryBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return entryBuilder.CreateAlloca(type, nullptr, name);
  }

  llvm::Value* emitBlock(const ast::BlockExpr& block) {
    llvm::Value* result = nullptr;
    for (const auto& expr : block.expressions) {
      if (builder_.GetInsertBlock()->getTerminator()) {
        break;
      }
      if (expr != nullptr) {
        result = emitExpr(*expr);
      }
    }
    return result;
  }

  llvm::Value* emitExpr(const ast::Expr& expr) {
    if (const auto* literal = dynamic_cast<const ast::LiteralExpr*>(&expr)) {
      return emitLiteral(*literal);
    }
    if (const auto* name = dynamic_cast<const ast::NameExpr*>(&expr)) {
      return emitName(*name);
    }
    if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
      return emitBinary(*binary);
    }
    if (const auto* binding = dynamic_cast<const ast::BindingExpr*>(&expr)) {
      return emitBinding(*binding);
    }
    if (const auto* assign = dynamic_cast<const ast::AssignExpr*>(&expr)) {
      return emitAssign(*assign);
    }
    if (const auto* field = dynamic_cast<const ast::FieldAccessExpr*>(&expr)) {
      return emitFieldAccess(*field);
    }
    if (const auto* array = dynamic_cast<const ast::ArrayLiteralExpr*>(&expr)) {
      return emitArrayLiteral(*array);
    }
    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expr)) {
      return emitIndexAccess(*index);
    }
    if (const auto* move = dynamic_cast<const ast::MoveExpr*>(&expr)) {
      return move->value ? emitExpr(*move->value) : llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    }
    if (const auto* newExpr = dynamic_cast<const ast::NewExpr*>(&expr)) {
      return emitNew(*newExpr);
    }
    if (const auto* freeze = dynamic_cast<const ast::FreezeExpr*>(&expr)) {
      return emitFreeze(*freeze);
    }
    if (const auto* merge = dynamic_cast<const ast::MergeExpr*>(&expr)) {
      return emitMerge(*merge);
    }
    if (const auto* region = dynamic_cast<const ast::RegionExpr*>(&expr)) {
      return emitRegion(*region);
    }
    if (const auto* ret = dynamic_cast<const ast::ReturnExpr*>(&expr)) {
      llvm::Value* value = ret->value ? emitExpr(*ret->value) : nullptr;
      if (builder_.GetInsertBlock()->getParent()->getReturnType()->isVoidTy()) {
        return builder_.CreateRetVoid();
      }
      return builder_.CreateRet(value);
    }
    if (const auto* ifExpr = dynamic_cast<const ast::IfExpr*>(&expr)) {
      return emitIf(*ifExpr);
    }
    if (const auto* whileExpr = dynamic_cast<const ast::WhileExpr*>(&expr)) {
      return emitWhile(*whileExpr);
    }
    if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
      return emitCall(*call);
    }
    if (const auto* block = dynamic_cast<const ast::BlockExpr*>(&expr)) {
      return emitBlock(*block);
    }
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
  }

  llvm::Value* emitName(const ast::NameExpr& name) {
    if (!values_.contains(name.name)) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    auto* slot = values_.at(name.name);
    return builder_.CreateLoad(slot->getAllocatedType(), slot, name.name);
  }

  llvm::Value* emitBinding(const ast::BindingExpr& binding) {
    llvm::Value* initializer = binding.initializer ? emitExpr(*binding.initializer) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    llvm::Type* slotType = initializer->getType();
    if (!slotType->isArrayTy() && binding.type != nullptr) {
      slotType = typeFromAst(binding.type.get());
    }
    auto* slot = createEntryAlloca(function, binding.name, slotType);
    if (initializer->getType()->isStructTy()) {
      builder_.CreateStore(initializer, slot);
    } else {
      builder_.CreateStore(initializer, slot);
    }
    values_[binding.name] = slot;
    if (binding.type != nullptr && binding.type->capability == ast::Capability::Iso && layouts_.contains(binding.type->name)) {
      regionStructTypes_[binding.name] = layouts_.at(binding.type->name).type;
    }
    return nullptr;
  }

  llvm::Value* emitNew(const ast::NewExpr& newExpr) {
    if (newExpr.type == nullptr || !layouts_.contains(newExpr.type->name)) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    const auto& layout = layouts_.at(newExpr.type->name);
    if (newExpr.type->capability == ast::Capability::Iso) {
      const auto size = module_.getDataLayout().getTypeAllocSize(layout.type);
      auto* sizeValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), size);
      auto* strategyValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), regionStrategyValue(newExpr.regionStrategy));
      auto* region = builder_.CreateCall(regionNewIso(), {sizeValue, strategyValue}, "region");
      auto* typeId = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), layout.typeId);
      builder_.CreateCall(regionSetBridgeType(), {region, typeId});
      return region;
    }
    llvm::Value* value = llvm::UndefValue::get(layout.type);
    for (const auto& init : newExpr.fields) {
      if (!layout.fields.contains(init.name)) {
        continue;
      }
      const auto& field = layout.fields.at(init.name);
      llvm::Value* fieldValue = init.value ? emitExpr(*init.value) : llvm::Constant::getNullValue(field.type);
      value = builder_.CreateInsertValue(value, fieldValue, {static_cast<unsigned>(field.index)}, init.name);
    }
    return value;
  }

  llvm::Value* emitFreeze(const ast::FreezeExpr& freeze) {
    llvm::Value* region = freeze.value ? emitExpr(*freeze.value) : llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    return builder_.CreateCall(regionFreeze(), {region}, "frozen");
  }

  llvm::Value* emitMerge(const ast::MergeExpr& merge) {
    llvm::Value* source = merge.value ? emitExpr(*merge.value) : llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    llvm::Value* target = activeRegions_.empty() ? llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_)) : activeRegions_.back();
    return builder_.CreateCall(regionMerge(), {target, source}, "merged");
  }

  llvm::Value* emitRegion(const ast::RegionExpr& regionExpr) {
    llvm::Value* region = regionExpr.source ? emitExpr(*regionExpr.source) : llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    const bool isEnter = regionExpr.kind == ast::RegionExpr::Kind::Enter;
    if (isEnter) {
      builder_.CreateCall(regionEnter(), {region});
      activeRegions_.push_back(region);
    } else {
      builder_.CreateCall(regionExplore(), {region});
    }

    llvm::Value* bridge = builder_.CreateCall(regionBridge(), {region}, "bridge");
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    auto* bridgeSlot = createEntryAlloca(function, regionExpr.bindingName, bridge->getType());
    builder_.CreateStore(bridge, bridgeSlot);
    bridgeSlots_[regionExpr.bindingName] = bridgeSlot;
    pointerValues_[regionExpr.bindingName] = bridge;
    if (const auto* sourceName = dynamic_cast<const ast::NameExpr*>(regionExpr.source.get())) {
      if (regionStructTypes_.contains(sourceName->name)) {
        pointerStructTypes_[regionExpr.bindingName] = regionStructTypes_.at(sourceName->name);
      }
    }

    llvm::Value* result = regionExpr.body ? emitBlock(*regionExpr.body) : nullptr;
    if (isEnter) {
      llvm::Value* currentBridge = builder_.CreateLoad(bridgeSlot->getAllocatedType(), bridgeSlot, regionExpr.bindingName + ".bridge");
      builder_.CreateCall(regionExit(), {region, currentBridge});
      activeRegions_.pop_back();
    } else {
      builder_.CreateCall(regionExploreExit(), {region});
    }

    bridgeSlots_.erase(regionExpr.bindingName);
    pointerValues_.erase(regionExpr.bindingName);
    pointerStructTypes_.erase(regionExpr.bindingName);
    return result;
  }

  llvm::Value* emitFieldAccess(const ast::FieldAccessExpr& field) {
    const FieldLayout* layout = nullptr;
    auto* slot = fieldSlot(field, &layout);
    if (slot == nullptr || layout == nullptr) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    return builder_.CreateLoad(layout->type, slot, field.fieldName);
  }

  llvm::Value* emitArrayLiteral(const ast::ArrayLiteralExpr& array) {
    auto* arrayType = llvm::ArrayType::get(llvm::Type::getInt64Ty(context_), array.elements.size());
    llvm::Value* value = llvm::UndefValue::get(arrayType);
    for (std::size_t i = 0; i < array.elements.size(); ++i) {
      llvm::Value* element = array.elements[i] ? emitExpr(*array.elements[i]) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
      value = builder_.CreateInsertValue(value, element, {static_cast<unsigned>(i)}, "array.elem");
    }
    return value;
  }

  llvm::Value* emitIndexAccess(const ast::IndexExpr& index) {
    auto* slot = indexSlot(index);
    if (slot == nullptr) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    return builder_.CreateLoad(llvm::Type::getInt64Ty(context_), slot, "array.load");
  }

  llvm::Value* indexSlot(const ast::IndexExpr& index) {
    const auto* object = dynamic_cast<const ast::NameExpr*>(index.object.get());
    if (object == nullptr) {
      return nullptr;
    }
    llvm::Value* indexValue = index.index ? emitExpr(*index.index) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    if (indexValue->getType()->isIntegerTy(32)) {
      indexValue = builder_.CreateSExt(indexValue, llvm::Type::getInt64Ty(context_), "idx.ext");
    }
    if (arrayPointers_.contains(object->name)) {
      return builder_.CreateInBoundsGEP(llvm::Type::getInt64Ty(context_), arrayPointers_.at(object->name), indexValue, "array.elem.addr");
    }
    if (!values_.contains(object->name)) {
      return nullptr;
    }
    auto* arraySlot = values_.at(object->name);
    auto* arrayType = llvm::dyn_cast<llvm::ArrayType>(arraySlot->getAllocatedType());
    if (arrayType == nullptr) {
      return nullptr;
    }
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
    return builder_.CreateInBoundsGEP(arrayType, arraySlot, {zero, indexValue}, "array.elem.addr");
  }

  llvm::Value* fieldSlot(const ast::FieldAccessExpr& field, const FieldLayout** outLayout = nullptr) {
    const auto* object = dynamic_cast<const ast::NameExpr*>(field.object.get());
    if (object == nullptr) {
      return nullptr;
    }
    llvm::Value* objectAddress = nullptr;
    llvm::StructType* structType = nullptr;
    if (values_.contains(object->name)) {
      auto* objectSlot = values_.at(object->name);
      structType = llvm::dyn_cast<llvm::StructType>(objectSlot->getAllocatedType());
      objectAddress = objectSlot;
    } else if (pointerValues_.contains(object->name) && pointerStructTypes_.contains(object->name)) {
      objectAddress = pointerValues_.at(object->name);
      structType = pointerStructTypes_.at(object->name);
    }
    if (objectAddress == nullptr || structType == nullptr || !structType->hasName()) {
      return nullptr;
    }
    const std::string structName = structType->getName().str();
    if (!layouts_.contains(structName) || !layouts_.at(structName).fields.contains(field.fieldName)) {
      return nullptr;
    }
    const auto& fieldLayout = layouts_.at(structName).fields.at(field.fieldName);
    if (outLayout != nullptr) {
      *outLayout = &fieldLayout;
    }
    return builder_.CreateStructGEP(structType, objectAddress, static_cast<unsigned>(fieldLayout.index), field.fieldName + ".addr");
  }

  llvm::Value* emitAssign(const ast::AssignExpr& assign) {
    llvm::Value* value = assign.value ? emitExpr(*assign.value) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    if (const auto* target = dynamic_cast<const ast::NameExpr*>(assign.target.get())) {
      if (bridgeSlots_.contains(target->name)) {
        builder_.CreateStore(value, bridgeSlots_.at(target->name));
        pointerValues_[target->name] = value;
        return value;
      }
      if (!values_.contains(target->name)) {
        return nullptr;
      }
      builder_.CreateStore(value, values_.at(target->name));
      return value;
    }
    if (const auto* field = dynamic_cast<const ast::FieldAccessExpr*>(assign.target.get())) {
      auto* slot = fieldSlot(*field);
      if (slot == nullptr) {
        return nullptr;
      }
      builder_.CreateStore(value, slot);
      return value;
    }
    if (const auto* index = dynamic_cast<const ast::IndexExpr*>(assign.target.get())) {
      auto* slot = indexSlot(*index);
      if (slot == nullptr) {
        return nullptr;
      }
      builder_.CreateStore(value, slot);
      return value;
    }
    return nullptr;
  }

  llvm::Value* emitLiteral(const ast::LiteralExpr& literal) {
    if (literal.kind == ast::LiteralKind::Bool) {
      return literal.value == "true" ? llvm::ConstantInt::getTrue(context_) : llvm::ConstantInt::getFalse(context_);
    }
    if (literal.kind == ast::LiteralKind::Integer) {
      const bool isI32 = literal.suffix == "i32";
      const std::size_t suffixSize = literal.suffix.empty() ? 0 : literal.suffix.size();
      const std::string_view text(literal.value.data(), literal.value.size() - suffixSize);
      std::int64_t value = 0;
      std::from_chars(text.data(), text.data() + text.size(), value);
      return llvm::ConstantInt::get(isI32 ? llvm::Type::getInt32Ty(context_) : llvm::Type::getInt64Ty(context_), value, true);
    }
    if (literal.kind == ast::LiteralKind::Float) {
      const bool isF32 = literal.suffix == "f32";
      const std::size_t suffixSize = literal.suffix.empty() ? 0 : literal.suffix.size();
      const std::string text(literal.value.data(), literal.value.size() - suffixSize);
      return llvm::ConstantFP::get(isF32 ? llvm::Type::getFloatTy(context_) : llvm::Type::getDoubleTy(context_), std::strtod(text.c_str(), nullptr));
    }
    if (literal.kind == ast::LiteralKind::String) {
      return builder_.CreateGlobalStringPtr(decodeStringLiteral(literal.value), ".str");
    }
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
  }

  llvm::Value* emitBinary(const ast::BinaryExpr& binary) {
    llvm::Value* lhs = binary.lhs ? emitExpr(*binary.lhs) : nullptr;
    llvm::Value* rhs = binary.rhs ? emitExpr(*binary.rhs) : nullptr;
    const bool isFloat = lhs != nullptr && lhs->getType()->isFloatingPointTy();
    if (binary.op == "+") {
      return isFloat ? builder_.CreateFAdd(lhs, rhs, "addtmp") : builder_.CreateNSWAdd(lhs, rhs, "addtmp");
    }
    if (binary.op == "-") {
      return isFloat ? builder_.CreateFSub(lhs, rhs, "subtmp") : builder_.CreateNSWSub(lhs, rhs, "subtmp");
    }
    if (binary.op == "*") {
      return isFloat ? builder_.CreateFMul(lhs, rhs, "multmp") : builder_.CreateNSWMul(lhs, rhs, "multmp");
    }
    if (binary.op == "/") {
      return isFloat ? builder_.CreateFDiv(lhs, rhs, "divtmp") : builder_.CreateSDiv(lhs, rhs, "divtmp");
    }
    if (binary.op == "%") {
      return builder_.CreateSRem(lhs, rhs, "modtmp");
    }
    if (binary.op == "==") {
      return isFloat ? builder_.CreateFCmpOEQ(lhs, rhs, "eqtmp") : builder_.CreateICmpEQ(lhs, rhs, "eqtmp");
    }
    if (binary.op == "!=") {
      return isFloat ? builder_.CreateFCmpONE(lhs, rhs, "netmp") : builder_.CreateICmpNE(lhs, rhs, "netmp");
    }
    if (binary.op == "<") {
      return isFloat ? builder_.CreateFCmpOLT(lhs, rhs, "lttmp") : builder_.CreateICmpSLT(lhs, rhs, "lttmp");
    }
    if (binary.op == "<=") {
      return isFloat ? builder_.CreateFCmpOLE(lhs, rhs, "letmp") : builder_.CreateICmpSLE(lhs, rhs, "letmp");
    }
    if (binary.op == ">") {
      return isFloat ? builder_.CreateFCmpOGT(lhs, rhs, "gttmp") : builder_.CreateICmpSGT(lhs, rhs, "gttmp");
    }
    if (binary.op == ">=") {
      return isFloat ? builder_.CreateFCmpOGE(lhs, rhs, "getmp") : builder_.CreateICmpSGE(lhs, rhs, "getmp");
    }
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
  }

  llvm::Value* emitCallArgument(const ast::Expr& argument, llvm::Type* expectedType) {
    if (!expectedType->isPointerTy()) {
      return emitExpr(argument);
    }
    const auto* name = dynamic_cast<const ast::NameExpr*>(&argument);
    if (name == nullptr) {
      return emitExpr(argument);
    }
    if (arrayPointers_.contains(name->name)) {
      return arrayPointers_.at(name->name);
    }
    if (!values_.contains(name->name)) {
      return emitExpr(argument);
    }
    auto* slot = values_.at(name->name);
    auto* arrayType = llvm::dyn_cast<llvm::ArrayType>(slot->getAllocatedType());
    if (arrayType == nullptr) {
      return emitExpr(argument);
    }
    auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
    return builder_.CreateInBoundsGEP(arrayType, slot, {zero, zero}, name->name + ".data");
  }

  llvm::Value* emitReadlnI64() {
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    auto* slot = createEntryAlloca(function, "readln.value", llvm::Type::getInt64Ty(context_));
    auto* format = builder_.CreateGlobalStringPtr("%lld", ".scanf.i64");
    builder_.CreateCall(scanfFunction(), {format, slot});
    return builder_.CreateLoad(llvm::Type::getInt64Ty(context_), slot, "readln.load");
  }

  llvm::Value* emitPrintCall(std::string_view name, const ast::List<ast::Expr>& callArguments) {
    if (name == "readln_i64") {
      return emitReadlnI64();
    }
    std::vector<llvm::Value*> arguments;
    if (name == "print") {
      arguments.push_back(builder_.CreateGlobalStringPtr("%s", ".printf.str"));
    } else if (name == "println") {
      arguments.push_back(builder_.CreateGlobalStringPtr("%s\n", ".printf.strln"));
    } else if (name == "print_i64") {
      arguments.push_back(builder_.CreateGlobalStringPtr("%lld", ".printf.i64"));
    } else if (name == "println_i64") {
      arguments.push_back(builder_.CreateGlobalStringPtr("%lld\n", ".printf.i64ln"));
    } else {
      return nullptr;
    }
    if (!callArguments.empty() && callArguments.front() != nullptr) {
      arguments.push_back(emitExpr(*callArguments.front()));
    }
    builder_.CreateCall(printfFunction(), arguments);
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context_));
  }

  llvm::Value* emitCall(const ast::CallExpr& call) {
    if (call.callee != nullptr) {
      if (auto builtinName = qualifiedCoreBuiltinName(*call.callee)) {
        if (auto* result = emitPrintCall(*builtinName, call.arguments)) {
          return result;
        }
      }
      ast::Path path = pathFromFieldAccess(*call.callee);
      if (!path.segments.empty()) {
        if (auto resolved = resolveFunctionKey(path)) {
          auto* function = functions_.at(*resolved);
          std::vector<llvm::Value*> arguments;
          arguments.reserve(call.arguments.size());
          for (std::size_t i = 0; i < call.arguments.size(); ++i) {
            llvm::Type* expectedType = i < function->arg_size() ? function->getFunctionType()->getParamType(i) : llvm::Type::getInt64Ty(context_);
            arguments.push_back(call.arguments[i] ? emitCallArgument(*call.arguments[i], expectedType) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
          }
          return builder_.CreateCall(function, arguments, function->getReturnType()->isVoidTy() ? "" : "calltmp");
        }
      }
    }
    if (const auto* methodCallee = dynamic_cast<const ast::FieldAccessExpr*>(call.callee.get())) {
      if (auto* result = emitMethodCall(*methodCallee, call.arguments)) {
        return result;
      }
    }
    const auto* callee = dynamic_cast<const ast::NameExpr*>(call.callee.get());
    if (callee == nullptr) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    if (auto* builtin = emitPrintCall(callee->name, call.arguments)) {
      return builtin;
    }
    if (functions_.contains(callee->name)) {
      auto* function = functions_.at(callee->name);
      std::vector<llvm::Value*> arguments;
      arguments.reserve(call.arguments.size());
      for (std::size_t i = 0; i < call.arguments.size(); ++i) {
        llvm::Type* expectedType = i < function->arg_size() ? function->getFunctionType()->getParamType(i) : llvm::Type::getInt64Ty(context_);
        arguments.push_back(call.arguments[i] ? emitCallArgument(*call.arguments[i], expectedType) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
      }
      return builder_.CreateCall(function, arguments, function->getReturnType()->isVoidTy() ? "" : "calltmp");
    }
    std::vector<llvm::Value*> arguments;
    arguments.reserve(call.arguments.size());
    for (const auto& argument : call.arguments) {
      arguments.push_back(argument ? emitExpr(*argument) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
    }
    if (callee->name == "puts") {
      return builder_.CreateCall(putsFunction(), arguments, "calltmp");
    }
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
  }

  llvm::Value* emitMethodCall(const ast::FieldAccessExpr& callee, const ast::List<ast::Expr>& callArguments) {
    const auto* object = dynamic_cast<const ast::NameExpr*>(callee.object.get());
    if (object == nullptr) {
      return nullptr;
    }
    if (layouts_.contains(object->name) && layouts_.at(object->name).associatedFunctions.contains(callee.fieldName)) {
      const auto& function = layouts_.at(object->name).associatedFunctions.at(callee.fieldName);
      std::vector<llvm::Value*> arguments;
      arguments.reserve(callArguments.size());
      for (const auto& argument : callArguments) {
        arguments.push_back(argument ? emitExpr(*argument) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
      }
      return builder_.CreateCall(function.function, arguments, "calltmp");
    }
    if (!values_.contains(object->name)) {
      return nullptr;
    }
    auto* objectSlot = values_.at(object->name);
    auto* structType = llvm::dyn_cast<llvm::StructType>(objectSlot->getAllocatedType());
    if (structType == nullptr || !structType->hasName()) {
      return nullptr;
    }
    const std::string structName = structType->getName().str();
    if (!layouts_.contains(structName) || !layouts_.at(structName).methods.contains(callee.fieldName)) {
      return nullptr;
    }
    const auto& method = layouts_.at(structName).methods.at(callee.fieldName);
    std::vector<llvm::Value*> arguments;
    arguments.reserve(callArguments.size() + 1);
    arguments.push_back(objectSlot);
    for (const auto& argument : callArguments) {
      arguments.push_back(argument ? emitExpr(*argument) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
    }
    return builder_.CreateCall(method.function, arguments, "calltmp");
  }

  llvm::Value* emitWhile(const ast::WhileExpr& whileExpr) {
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    auto* conditionBlock = llvm::BasicBlock::Create(context_, "while.cond", function);
    auto* bodyBlock = llvm::BasicBlock::Create(context_, "while.body");
    auto* exitBlock = llvm::BasicBlock::Create(context_, "while.end");
    builder_.CreateBr(conditionBlock);

    builder_.SetInsertPoint(conditionBlock);
    llvm::Value* condition = whileExpr.condition ? emitExpr(*whileExpr.condition) : llvm::ConstantInt::getFalse(context_);
    builder_.CreateCondBr(condition, bodyBlock, exitBlock);

    function->getBasicBlockList().push_back(bodyBlock);
    builder_.SetInsertPoint(bodyBlock);
    if (whileExpr.body != nullptr) {
      emitBlock(*whileExpr.body);
    }
    if (!builder_.GetInsertBlock()->getTerminator()) {
      builder_.CreateBr(conditionBlock);
    }

    function->getBasicBlockList().push_back(exitBlock);
    builder_.SetInsertPoint(exitBlock);
    return nullptr;
  }

  llvm::Value* emitIf(const ast::IfExpr& ifExpr) {
    llvm::Value* condition = ifExpr.condition ? emitExpr(*ifExpr.condition) : llvm::ConstantInt::getFalse(context_);
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    auto* thenBlock = llvm::BasicBlock::Create(context_, "if.then", function);
    auto* elseBlock = llvm::BasicBlock::Create(context_, "if.else");
    auto* mergeBlock = llvm::BasicBlock::Create(context_, "if.end");
    builder_.CreateCondBr(condition, thenBlock, elseBlock);

    builder_.SetInsertPoint(thenBlock);
    if (ifExpr.thenBlock != nullptr) {
      emitBlock(*ifExpr.thenBlock);
    }
    if (!builder_.GetInsertBlock()->getTerminator()) {
      builder_.CreateBr(mergeBlock);
    }

    function->getBasicBlockList().push_back(elseBlock);
    builder_.SetInsertPoint(elseBlock);
    if (ifExpr.elseBranch != nullptr) {
      emitExpr(*ifExpr.elseBranch);
    }
    if (!builder_.GetInsertBlock()->getTerminator()) {
      builder_.CreateBr(mergeBlock);
    }

    function->getBasicBlockList().push_back(mergeBlock);
    builder_.SetInsertPoint(mergeBlock);
    return nullptr;
  }

  llvm::LLVMContext context_;
  llvm::Module module_;
  llvm::IRBuilder<> builder_;
  std::unordered_map<std::string, llvm::Function*> functions_;
  std::unordered_map<std::string, std::vector<std::string>> simpleFunctionKeys_;
  const ast::Decl* currentDecl_ = nullptr;
  std::unordered_map<std::string, llvm::AllocaInst*> values_;
  std::unordered_map<std::string, llvm::Value*> pointerValues_;
  std::unordered_map<std::string, llvm::Value*> arrayPointers_;
  std::unordered_map<std::string, llvm::StructType*> pointerStructTypes_;
  std::unordered_map<std::string, llvm::StructType*> regionStructTypes_;
  std::unordered_map<std::string, llvm::AllocaInst*> bridgeSlots_;
  std::vector<llvm::Value*> activeRegions_;
  std::unordered_map<std::string, StructLayout> layouts_;
};

} // namespace
#endif

bool LLVMCodeGen::isAvailable() {
#if GURA_HAS_LLVM
  return true;
#else
  return false;
#endif
}

std::string LLVMCodeGen::emitModule(const ast::SourceFile& file) const {
#if GURA_HAS_LLVM
  ModuleEmitter emitter;
  return emitter.emit(file);
#else
  (void)file;
  return "; LLVM support not found at configure time\n";
#endif
}

} // namespace gura
