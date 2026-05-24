#include "gura/codegen/LLVMCodeGen.h"

#include "gura/ast/Expr.h"

#if GURA_HAS_LLVM
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>
#endif

#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gura {

#if GURA_HAS_LLVM
namespace {

class ModuleEmitter {
public:
  ModuleEmitter() : module_("gura", context_), builder_(context_) {}

  std::string emit(const ast::SourceFile& file) {
    for (const auto& decl : file.declarations) {
      if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
        declareFunction(*fn);
      }
    }
    for (const auto& decl : file.declarations) {
      if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get())) {
        emitFunction(*fn);
      }
    }
    std::string output;
    llvm::raw_string_ostream stream(output);
    module_.print(stream, nullptr);
    return output;
  }

private:
  llvm::Type* typeFromAst(const ast::TypeRef* type) {
    if (type == nullptr || type->name == "unit") {
      return llvm::Type::getVoidTy(context_);
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
    return llvm::Type::getInt64Ty(context_);
  }

  void declareFunction(const ast::FnDecl& fn) {
    std::vector<llvm::Type*> params;
    params.reserve(fn.params.size());
    for (const auto& param : fn.params) {
      params.push_back(typeFromAst(param.type.get()));
    }
    auto* fnType = llvm::FunctionType::get(typeFromAst(fn.returnType.get()), params, false);
    auto* function = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fn.name, module_);
    std::size_t index = 0;
    for (auto& arg : function->args()) {
      arg.setName(fn.params[index++].name);
    }
    functions_.emplace(fn.name, function);
  }

  void emitFunction(const ast::FnDecl& fn) {
    llvm::Function* function = functions_.at(fn.name);
    auto* entry = llvm::BasicBlock::Create(context_, "entry", function);
    builder_.SetInsertPoint(entry);
    values_.clear();
    for (auto& arg : function->args()) {
      auto* slot = createEntryAlloca(function, arg.getName().str(), arg.getType());
      builder_.CreateStore(&arg, slot);
      values_.emplace(std::string(arg.getName()), slot);
    }
    if (fn.body != nullptr) {
      emitBlock(*fn.body);
    }
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
    auto* slot = createEntryAlloca(function, binding.name, initializer->getType());
    builder_.CreateStore(initializer, slot);
    values_[binding.name] = slot;
    return nullptr;
  }

  llvm::Value* emitAssign(const ast::AssignExpr& assign) {
    const auto* target = dynamic_cast<const ast::NameExpr*>(assign.target.get());
    if (target == nullptr || !values_.contains(target->name)) {
      return nullptr;
    }
    llvm::Value* value = assign.value ? emitExpr(*assign.value) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    builder_.CreateStore(value, values_.at(target->name));
    return value;
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

  llvm::Value* emitCall(const ast::CallExpr& call) {
    const auto* callee = dynamic_cast<const ast::NameExpr*>(call.callee.get());
    if (callee == nullptr || !functions_.contains(callee->name)) {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true);
    }
    std::vector<llvm::Value*> arguments;
    arguments.reserve(call.arguments.size());
    for (const auto& argument : call.arguments) {
      arguments.push_back(argument ? emitExpr(*argument) : llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0, true));
    }
    return builder_.CreateCall(functions_.at(callee->name), arguments, "calltmp");
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
  std::unordered_map<std::string, llvm::AllocaInst*> values_;
};

} // namespace
#endif

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
