#include "gura/hir/Sema.h"

#include "gura/ast/Expr.h"

#include <fmt/format.h>

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

namespace gura::hir {

namespace {

Type invalidType() {
  return Type{Capability::None, "<invalid>"};
}

Type unitType() {
  return Type{Capability::None, "unit"};
}

Type boolType() {
  return Type{Capability::None, "bool"};
}

Type noneType() {
  return Type{Capability::None, "none"};
}

Type cstringType() {
  return Type{Capability::None, "cstring"};
}

Type numericType(std::string name) {
  return Type{Capability::None, std::move(name)};
}

Type arrayType(Capability capability, Type elementType, std::optional<std::size_t> length = std::nullopt) {
  Type type;
  type.capability = capability;
  type.name = "array";
  type.isArray = true;
  type.elementType = std::make_shared<Type>(std::move(elementType));
  type.arrayLength = length;
  return type;
}

std::string typeName(const Type& type) {
  if (!type.isArray) {
    return type.name;
  }
  const std::string element = type.elementType ? typeName(*type.elementType) : "<invalid>";
  if (type.arrayLength.has_value()) {
    return fmt::format("[{}; {}]", element, *type.arrayLength);
  }
  return fmt::format("[{}]", element);
}

bool isPlainI64(const Type& type) {
  return !type.isArray && type.capability == Capability::None && type.name == "i64";
}

bool isIntegerType(const Type& type) {
  return type.capability == Capability::None && (type.name == "i32" || type.name == "i64");
}

bool isFloatType(const Type& type) {
  return type.capability == Capability::None && (type.name == "f32" || type.name == "f64");
}

bool isNumericType(const Type& type) {
  return isIntegerType(type) || isFloatType(type);
}

bool isRegionStrategy(std::string_view strategy) {
  return strategy == "Arena" || strategy == "RC" || strategy == "GC" || strategy == "Manual";
}

bool isCoreBuiltinName(std::string_view name) {
  return name == "print" || name == "println" || name == "print_i64" || name == "println_i64" || name == "readln_i64";
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

bool sameNominalType(const Type& lhs, const Type& rhs) {
  if (lhs.name == "<invalid>" || rhs.name == "<invalid>") {
    return true;
  }
  if (lhs.isArray || rhs.isArray) {
    if (!lhs.isArray || !rhs.isArray || lhs.elementType == nullptr || rhs.elementType == nullptr) {
      return false;
    }
    return sameNominalType(*lhs.elementType, *rhs.elementType);
  }
  return lhs.name == rhs.name;
}

bool isInvalid(const Type& type) {
  return type.name == "<invalid>";
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
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

const ast::NameExpr* asNameExpr(const ast::Expr* expr) {
  return dynamic_cast<const ast::NameExpr*>(expr);
}

} // namespace

bool Sema::check(const ast::SourceFile& file) {
  collectModules(file);
  collectStructs(file);
  collectTraits(file);
  collectImplMethods(file);
  collectFunctionSignatures(file);
  validateTraitImpls(file);
  bool ok = true;
  for (const auto& decl : file.declarations) {
    if (decl == nullptr) {
      ok = false;
      continue;
    }
    currentDecl_ = decl.get();
    ok = checkDecl(*decl) && ok;
    currentDecl_ = nullptr;
  }
  return ok && !diagnostics_.hasError();
}

void Sema::pushScope() {
  scopes_.emplace_back();
}

void Sema::popScope() {
  scopes_.pop_back();
}

void Sema::declare(std::string name, BindingState state) {
  if (scopes_.empty()) {
    pushScope();
  }
  auto& scope = scopes_.back();
  if (scope.contains(name)) {
    report(state.span, fmt::format("binding '{}' is already defined in this scope", name));
    return;
  }
  scope.emplace(std::move(name), std::move(state));
}

Sema::BindingState* Sema::lookup(const std::string& name) {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    if (auto it = scope->find(name); it != scope->end()) {
      return &it->second;
    }
  }
  return nullptr;
}

void Sema::collectModules(const ast::SourceFile& file) {
  modules_.clear();
  for (const auto& decl : file.declarations) {
    if (decl == nullptr || decl->modulePath.segments.empty()) {
      continue;
    }
    const std::string moduleName = joinPath(decl->modulePath.segments);
    auto& module = modules_[moduleName];
    module.path = decl->modulePath;
    module.imports = decl->imports;
  }
}

void Sema::collectStructs(const ast::SourceFile& file) {
  structs_.clear();
  for (const auto& decl : file.declarations) {
    const auto* structDecl = dynamic_cast<const ast::StructDecl*>(decl.get());
    if (structDecl == nullptr) {
      continue;
    }
    StructInfo info;
    info.span = structDecl->span;
    for (const auto& field : structDecl->fields) {
      if (info.fields.contains(field.name)) {
        report(field.span, fmt::format("field '{}' is already defined", field.name));
        continue;
      }
      Type fieldType = field.type ? typeFromAst(*field.type) : invalidType();
      if (fieldType.isArray) {
        report(field.type ? field.type->span : field.span, "array fields are not supported yet");
      }
      info.fields.emplace(field.name, FieldInfo{std::move(fieldType), field.isVar, field.span});
    }
    for (const auto& method : structDecl->methods) {
      if (method != nullptr) {
        collectMethod(info, *method, structDecl->name, true);
      }
    }
    if (structs_.contains(structDecl->name)) {
      report(structDecl->span, fmt::format("struct '{}' is already defined", structDecl->name));
      continue;
    }
    structs_.emplace(structDecl->name, std::move(info));
  }
}

void Sema::collectTraits(const ast::SourceFile& file) {
  traits_.clear();
  for (const auto& decl : file.declarations) {
    const auto* traitDecl = dynamic_cast<const ast::TraitDecl*>(decl.get());
    if (traitDecl == nullptr) {
      continue;
    }
    if (structs_.contains(traitDecl->name)) {
      report(traitDecl->span, fmt::format("trait '{}' conflicts with a struct of the same name", traitDecl->name));
      continue;
    }
    if (traits_.contains(traitDecl->name)) {
      report(traitDecl->span, fmt::format("trait '{}' is already defined", traitDecl->name));
      continue;
    }
    TraitInfo info;
    info.span = traitDecl->span;
    for (const auto& method : traitDecl->methods) {
      if (method != nullptr) {
        collectTraitMethod(info, *method, traitDecl->name);
      }
    }
    traits_.emplace(traitDecl->name, std::move(info));
  }
}

void Sema::collectImplMethods(const ast::SourceFile& file) {
  for (const auto& decl : file.declarations) {
    const auto* implDecl = dynamic_cast<const ast::ImplDecl*>(decl.get());
    if (implDecl == nullptr) {
      continue;
    }
    if (implDecl->traitName.has_value() && !traits_.contains(*implDecl->traitName)) {
      report(implDecl->span, fmt::format("trait '{}' is not defined", *implDecl->traitName));
    }
    auto structIt = structs_.find(implDecl->typeName);
    if (structIt == structs_.end()) {
      report(implDecl->span, fmt::format("impl target type '{}' is not defined", implDecl->typeName));
      continue;
    }
    for (const auto& method : implDecl->methods) {
      if (method != nullptr) {
        collectMethod(structIt->second, *method, implDecl->typeName, false);
      }
    }
  }
}

std::optional<Sema::MethodInfo> Sema::makeMethodInfo(const ast::FnDecl& method, const std::string& selfTypeName, bool requireReceiver) {
  if (!method.genericParams.empty()) {
    report(method.span, "generic methods are not supported yet");
    return std::nullopt;
  }
  const bool hasReceiver = !method.params.empty() && method.params.front().name == "self";
  if (requireReceiver && (!hasReceiver || method.params.front().type == nullptr)) {
    report(method.span, fmt::format("method '{}' must declare self as its first parameter", method.name));
    return std::nullopt;
  }
  MethodInfo methodInfo;
  methodInfo.span = method.span;
  methodInfo.hasReceiver = hasReceiver;
  if (hasReceiver) {
    if (method.params.front().type == nullptr) {
      report(method.span, fmt::format("method '{}' must declare self as its first parameter", method.name));
      return std::nullopt;
    }
    methodInfo.receiverType = typeFromAst(*method.params.front().type);
    methodInfo.receiverType.name = selfTypeName;
  }
  methodInfo.returnType = method.returnType ? typeFromAst(*method.returnType) : unitType();
  methodInfo.decl = &method;
  const std::size_t firstExplicitParam = hasReceiver ? 1 : 0;
  for (std::size_t i = firstExplicitParam; i < method.params.size(); ++i) {
    methodInfo.params.push_back(method.params[i].type ? typeFromAst(*method.params[i].type) : invalidType());
  }
  return methodInfo;
}

void Sema::collectMethod(StructInfo& info, const ast::FnDecl& method, const std::string& selfTypeName, bool requireReceiver) {
  if (info.methods.contains(method.name)) {
    report(method.span, fmt::format("method '{}' is already defined", method.name));
    return;
  }
  auto methodInfo = makeMethodInfo(method, selfTypeName, requireReceiver);
  if (!methodInfo.has_value()) {
    return;
  }
  info.methods.emplace(method.name, std::move(*methodInfo));
}

void Sema::collectTraitMethod(TraitInfo& info, const ast::FnDecl& method, const std::string& traitName) {
  if (info.methods.contains(method.name)) {
    report(method.span, fmt::format("method '{}' is already defined in trait '{}'", method.name, traitName));
    return;
  }
  if (method.body != nullptr) {
    report(method.span, fmt::format("trait method '{}' must not have a body", method.name));
    return;
  }
  auto methodInfo = makeMethodInfo(method, "<Self>", true);
  if (!methodInfo.has_value()) {
    return;
  }
  info.methods.emplace(method.name, std::move(*methodInfo));
}

void Sema::validateTraitImpls(const ast::SourceFile& file) {
  for (const auto& decl : file.declarations) {
    const auto* implDecl = dynamic_cast<const ast::ImplDecl*>(decl.get());
    if (implDecl == nullptr || !implDecl->traitName.has_value()) {
      continue;
    }
    auto traitIt = traits_.find(*implDecl->traitName);
    auto structIt = structs_.find(implDecl->typeName);
    if (traitIt == traits_.end() || structIt == structs_.end()) {
      continue;
    }
    std::unordered_map<std::string, const ast::FnDecl*> implMethods;
    for (const auto& method : implDecl->methods) {
      if (method == nullptr) {
        continue;
      }
      implMethods.emplace(method->name, method.get());
    }
    for (const auto& [methodName, expected] : traitIt->second.methods) {
      auto implIt = implMethods.find(methodName);
      if (implIt == implMethods.end()) {
        report(implDecl->span, fmt::format("impl of trait '{}' for type '{}' is missing method '{}'", *implDecl->traitName, implDecl->typeName, methodName));
        continue;
      }
      auto actual = makeMethodInfo(*implIt->second, implDecl->typeName, true);
      if (!actual.has_value()) {
        continue;
      }
      if (!sameMethodSignature(expected, *actual)) {
        report(implIt->second->span, fmt::format("method '{}' signature does not match trait '{}'", methodName, *implDecl->traitName));
      }
    }
    for (const auto& [methodName, method] : implMethods) {
      if (!traitIt->second.methods.contains(methodName)) {
        report(method->span, fmt::format("method '{}' is not a member of trait '{}'", methodName, *implDecl->traitName));
      }
    }
  }
}

void Sema::collectFunctionSignatures(const ast::SourceFile& file) {
  functions_.clear();
  qualifiedFunctionKeys_.clear();
  simpleFunctionKeys_.clear();
  auto addBuiltin = [this](std::string name, Type returnType, std::vector<Type> params) {
    FunctionSignature signature;
    signature.returnType = std::move(returnType);
    signature.params = std::move(params);
    functions_.emplace(name, std::move(signature));
    simpleFunctionKeys_[name].push_back(name);
    qualifiedFunctionKeys_[fmt::format("std.core.{}", name)] = name;
  };
  addBuiltin("puts", numericType("i32"), {cstringType()});
  addBuiltin("print", unitType(), {cstringType()});
  addBuiltin("println", unitType(), {cstringType()});
  addBuiltin("print_i64", unitType(), {numericType("i64")});
  addBuiltin("println_i64", unitType(), {numericType("i64")});
  addBuiltin("readln_i64", numericType("i64"), {});
  for (const auto& decl : file.declarations) {
    const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get());
    if (fn == nullptr) {
      continue;
    }
    FunctionSignature signature;
    signature.span = fn->span;
    signature.genericParams = fn->genericParams;
    signature.returnType = fn->returnType ? typeFromAst(*fn->returnType) : unitType();
    if (signature.returnType.isArray) {
      report(fn->returnType ? fn->returnType->span : fn->span, "array return types are not supported yet");
    }
    for (const auto& param : fn->params) {
      Type paramType = param.type ? typeFromAst(*param.type) : invalidType();
      if (paramType.isArray && paramType.capability != Capability::Mut) {
        report(param.type ? param.type->span : fn->span, "array parameters currently require mut [i64]");
      }
      signature.params.push_back(std::move(paramType));
    }
    const std::string key = fn->modulePath.segments.empty() ? fn->name : qualifiedName(fn->modulePath, fn->name);
    if (functions_.contains(key)) {
      report(fn->span, fmt::format("function '{}' is already defined", key));
      continue;
    }
    if (fn->modulePath.segments.empty() && functions_.contains(fn->name)) {
      report(fn->span, fmt::format("function '{}' is already defined", fn->name));
      continue;
    }
    functions_.emplace(key, std::move(signature));
    simpleFunctionKeys_[fn->name].push_back(key);
    if (!fn->modulePath.segments.empty()) {
      qualifiedFunctionKeys_[key] = key;
    }
  }
}

Sema::GenericEnv Sema::makeGenericEnv(const ast::FnDecl& fn) {
  GenericEnv env;
  for (const auto& param : fn.genericParams) {
    if (env.contains(param.name)) {
      report(param.span, fmt::format("generic parameter '{}' is already defined", param.name));
      continue;
    }
    if (structs_.contains(param.name)) {
      report(param.span, fmt::format("generic parameter '{}' conflicts with a struct of the same name", param.name));
      continue;
    }
    if (isBuiltinTypeName(param.name)) {
      report(param.span, fmt::format("generic parameter '{}' conflicts with a builtin type name", param.name));
      continue;
    }
    GenericParamInfo info;
    info.span = param.span;
    for (const auto& bound : param.bounds) {
      if (!traits_.contains(bound.name)) {
        report(bound.span, fmt::format("trait '{}' is not defined", bound.name));
        continue;
      }
      if (containsString(info.bounds, bound.name)) {
        report(bound.span, fmt::format("trait bound '{}' is repeated for generic parameter '{}'", bound.name, param.name));
        continue;
      }
      info.bounds.push_back(bound.name);
    }
    env.emplace(param.name, std::move(info));
  }
  return env;
}

bool Sema::isGenericTypeName(const std::string& name) const {
  return currentGenericParams_.contains(name);
}

const Sema::GenericParamInfo* Sema::lookupGenericParam(const std::string& name) const {
  auto it = currentGenericParams_.find(name);
  return it == currentGenericParams_.end() ? nullptr : &it->second;
}

bool Sema::isBuiltinTypeName(const std::string& name) const {
  return name == "unit" || name == "bool" || name == "none" || name == "i32" || name == "i64" || name == "f32" || name == "f64";
}

bool Sema::checkDecl(const ast::Decl& decl) {
  if (const auto* structDecl = dynamic_cast<const ast::StructDecl*>(&decl)) {
    bool ok = true;
    for (const auto& method : structDecl->methods) {
      if (method != nullptr) {
        ok = checkMethod(*method, structDecl->name) && ok;
      }
    }
    return ok && !diagnostics_.hasError();
  }
  if (dynamic_cast<const ast::TraitDecl*>(&decl) != nullptr) {
    return !diagnostics_.hasError();
  }
  if (const auto* implDecl = dynamic_cast<const ast::ImplDecl*>(&decl)) {
    bool ok = true;
    for (const auto& method : implDecl->methods) {
      if (method != nullptr) {
        ok = checkMethod(*method, implDecl->typeName) && ok;
      }
    }
    return ok && !diagnostics_.hasError();
  }
  if (const auto* fn = dynamic_cast<const ast::FnDecl*>(&decl)) {
    return checkFn(*fn);
  }
  report(decl.span, "unsupported declaration");
  return false;
}

bool Sema::checkFn(const ast::FnDecl& fn) {
  const Type previousReturnType = currentReturnType_;
  const GenericEnv previousGenericParams = currentGenericParams_;
  currentGenericParams_ = makeGenericEnv(fn);
  currentReturnType_ = fn.returnType ? typeFromAst(*fn.returnType) : unitType();
  pushScope();
  for (const auto& param : fn.params) {
    if (param.type == nullptr) {
      continue;
    }
    declare(param.name, BindingState{typeFromAst(*param.type), false, false, param.type->span, BindingOrigin::Ordinary, regionDepth_});
  }
  if (fn.body != nullptr) {
    checkBlock(*fn.body);
  }
  popScope();
  currentGenericParams_ = previousGenericParams;
  currentReturnType_ = previousReturnType;
  return !diagnostics_.hasError();
}

bool Sema::checkMethod(const ast::FnDecl& fn, const std::string& selfTypeName) {
  const Type previousReturnType = currentReturnType_;
  currentReturnType_ = fn.returnType ? typeFromAst(*fn.returnType) : unitType();
  pushScope();
  for (std::size_t i = 0; i < fn.params.size(); ++i) {
    const auto& param = fn.params[i];
    if (param.type == nullptr) {
      continue;
    }
    Type paramType = typeFromAst(*param.type);
    if (i == 0 && param.name == "self") {
      paramType.name = selfTypeName;
    }
    declare(param.name, BindingState{paramType, false, false, param.type->span, BindingOrigin::Ordinary, regionDepth_});
  }
  if (fn.body != nullptr) {
    checkBlock(*fn.body);
  }
  popScope();
  currentReturnType_ = previousReturnType;
  return !diagnostics_.hasError();
}

Type Sema::checkBlock(const ast::BlockExpr& block) {
  pushScope();
  Type result = unitType();
  for (const auto& expr : block.expressions) {
    if (expr != nullptr) {
      result = checkExpr(*expr);
    }
  }
  popScope();
  return result;
}

Type Sema::checkExpr(const ast::Expr& expr, ValueContext context) {
  if (const auto* literal = dynamic_cast<const ast::LiteralExpr*>(&expr)) {
    switch (literal->kind) {
    case ast::LiteralKind::Bool: return boolType();
    case ast::LiteralKind::None: return noneType();
    case ast::LiteralKind::Integer: return numericType(literal->suffix.empty() ? "i64" : literal->suffix);
    case ast::LiteralKind::Float: return numericType(literal->suffix.empty() ? "f64" : literal->suffix);
    case ast::LiteralKind::String: return cstringType();
    case ast::LiteralKind::Char: return invalidType();
    }
  }
  if (const auto* name = dynamic_cast<const ast::NameExpr*>(&expr)) {
    return checkName(*name, context);
  }
  if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
    return checkBinary(*binary);
  }
  if (const auto* assign = dynamic_cast<const ast::AssignExpr*>(&expr)) {
    return checkAssign(*assign);
  }
  if (const auto* field = dynamic_cast<const ast::FieldAccessExpr*>(&expr)) {
    return checkFieldAccess(*field);
  }
  if (const auto* array = dynamic_cast<const ast::ArrayLiteralExpr*>(&expr)) {
    return checkArrayLiteral(*array);
  }
  if (const auto* index = dynamic_cast<const ast::IndexExpr*>(&expr)) {
    return checkIndex(*index);
  }
  if (const auto* binding = dynamic_cast<const ast::BindingExpr*>(&expr)) {
    return checkBinding(*binding);
  }
  if (const auto* move = dynamic_cast<const ast::MoveExpr*>(&expr)) {
    return checkMove(*move);
  }
  if (const auto* newExpr = dynamic_cast<const ast::NewExpr*>(&expr)) {
    return checkNew(*newExpr);
  }
  if (const auto* freeze = dynamic_cast<const ast::FreezeExpr*>(&expr)) {
    return checkFreeze(*freeze);
  }
  if (const auto* merge = dynamic_cast<const ast::MergeExpr*>(&expr)) {
    return checkMerge(*merge);
  }
  if (const auto* ifExpr = dynamic_cast<const ast::IfExpr*>(&expr)) {
    return checkIf(*ifExpr);
  }
  if (const auto* whileExpr = dynamic_cast<const ast::WhileExpr*>(&expr)) {
    return checkWhile(*whileExpr);
  }
  if (const auto* region = dynamic_cast<const ast::RegionExpr*>(&expr)) {
    return checkRegion(*region);
  }
  if (const auto* ret = dynamic_cast<const ast::ReturnExpr*>(&expr)) {
    return checkReturn(*ret);
  }
  if (const auto* block = dynamic_cast<const ast::BlockExpr*>(&expr)) {
    return checkBlock(*block);
  }
  if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
    return checkCall(*call);
  }
  report(expr.span, "unsupported expression");
  return invalidType();
}

Type Sema::checkName(const ast::NameExpr& expr, ValueContext context) {
  auto* binding = lookup(expr.name);
  if (binding == nullptr) {
    report(expr.span, fmt::format("undefined binding '{}'", expr.name));
    return invalidType();
  }
  if (binding->moved) {
    report(expr.span, fmt::format("binding '{}' was already moved", expr.name));
    return invalidType();
  }
  if (context == ValueContext::Ordinary && binding->type.capability == Capability::Iso) {
    report(expr.span, fmt::format("cannot copy iso binding '{}'; use move", expr.name));
    return invalidType();
  }
  return binding->type;
}

Type Sema::checkMove(const ast::MoveExpr& expr) {
  const auto* name = dynamic_cast<const ast::NameExpr*>(expr.value.get());
  if (name == nullptr) {
    report(expr.span, "move currently requires a binding name");
    return invalidType();
  }
  auto* binding = lookup(name->name);
  if (binding == nullptr) {
    report(name->span, fmt::format("undefined binding '{}'", name->name));
    return invalidType();
  }
  if (binding->moved) {
    report(name->span, fmt::format("binding '{}' was already moved", name->name));
    return invalidType();
  }
  binding->moved = true;
  return binding->type;
}

Type Sema::checkNew(const ast::NewExpr& expr) {
  Type result = expr.type ? typeFromAst(*expr.type) : invalidType();
  if (!expr.regionStrategy.empty()) {
    if (result.capability != Capability::Iso) {
      report(expr.span, "region allocation strategy requires an iso allocation");
    } else if (!isRegionStrategy(expr.regionStrategy)) {
      report(expr.span, fmt::format("unknown region allocation strategy '{}'", expr.regionStrategy));
    }
  }
  if (isInvalid(result) || expr.fields.empty()) {
    return result;
  }
  const auto structIt = structs_.find(result.name);
  if (structIt == structs_.end()) {
    report(expr.type ? expr.type->span : expr.span, fmt::format("type '{}' has no fields", result.name));
    return invalidType();
  }
  std::unordered_set<std::string> initialized;
  for (const auto& init : expr.fields) {
    const auto [_, inserted] = initialized.insert(init.name);
    if (!inserted) {
      report(init.span, fmt::format("field '{}' is initialized more than once", init.name));
      continue;
    }
    const auto fieldIt = structIt->second.fields.find(init.name);
    if (fieldIt == structIt->second.fields.end()) {
      report(init.span, fmt::format("type '{}' has no field '{}'", result.name, init.name));
      if (init.value != nullptr) {
        checkExpr(*init.value);
      }
      continue;
    }
    Type value = init.value ? checkExpr(*init.value) : unitType();
    if (!sameType(fieldIt->second.type, value)) {
      report(init.value ? init.value->span : init.span, fmt::format("initializer type '{}' does not match field type '{}'", value.name, fieldIt->second.type.name));
    }
  }
  for (const auto& [fieldName, fieldInfo] : structIt->second.fields) {
    if (!initialized.contains(fieldName)) {
      report(expr.span, fmt::format("missing initializer for field '{}'", fieldName));
    }
  }
  return result;
}

Type Sema::checkBinding(const ast::BindingExpr& expr) {
  Type init = expr.initializer ? checkExpr(*expr.initializer) : unitType();
  Type declared = expr.type ? typeFromAst(*expr.type) : init;
  if (expr.type != nullptr && !sameNominalType(declared, init)) {
    report(expr.span, fmt::format("initializer type '{}' does not match declared type '{}'", typeName(init), typeName(declared)));
  }
  if (declared.isArray && init.isArray && init.arrayLength.has_value()) {
    declared.arrayLength = init.arrayLength;
  }
  BindingOrigin origin = isRegionLocal(init.capability) && regionDepth_ > 0 ? BindingOrigin::RegionLocal : BindingOrigin::Ordinary;
  declare(expr.name, BindingState{declared, expr.isVar, false, expr.span, origin, regionDepth_});
  return unitType();
}

Type Sema::checkAssign(const ast::AssignExpr& expr) {
  Type value = expr.value ? checkExpr(*expr.value) : unitType();
  if (const auto* name = dynamic_cast<const ast::NameExpr*>(expr.target.get())) {
    auto* binding = lookup(name->name);
    if (binding == nullptr) {
      report(name->span, fmt::format("undefined binding '{}'", name->name));
      return invalidType();
    }
    if (!binding->isVar) {
      report(expr.span, fmt::format("cannot assign to let binding '{}'", name->name));
      return invalidType();
    }
    if (binding->bridgeSlot) {
      if (binding->type.capability != Capability::Mut) {
        report(expr.span, fmt::format("cannot reassign explore bridge '{}'", name->name));
        return invalidType();
      }
      if (value.capability != Capability::Mut) {
        report(expr.value ? expr.value->span : expr.span, "bridge reassignment requires a mut value");
        return invalidType();
      }
      if (binding->type.name != value.name) {
        report(expr.value ? expr.value->span : expr.span, "bridge reassignment must preserve bridge type in this milestone");
        return invalidType();
      }
      binding->bridgeReassigned = true;
      binding->moved = false;
      return unitType();
    }
    if (isRegionLocal(value.capability) && binding->regionDepth < regionDepth_) {
      report(expr.value ? expr.value->span : expr.span, "region-local value cannot escape its region");
      return invalidType();
    }
    if (!sameType(binding->type, value)) {
      report(expr.value ? expr.value->span : expr.span, fmt::format("assignment type '{}' does not match binding type '{}'", value.name, binding->type.name));
      return invalidType();
    }
    binding->moved = false;
    return unitType();
  }
  if (const auto* field = dynamic_cast<const ast::FieldAccessExpr*>(expr.target.get())) {
    return checkFieldAssign(*field, value, expr.value ? expr.value->span : expr.span);
  }
  if (const auto* index = dynamic_cast<const ast::IndexExpr*>(expr.target.get())) {
    return checkIndexAssign(*index, value, expr.value ? expr.value->span : expr.span);
  }
  report(expr.span, "assignment target must be a binding, field, or index");
  return invalidType();
}

Type Sema::checkFieldAccess(const ast::FieldAccessExpr& expr) {
  Type object = expr.object ? checkExpr(*expr.object, ValueContext::Operand) : invalidType();
  if (isInvalid(object)) {
    return invalidType();
  }
  if (object.capability == Capability::Iso) {
    report(expr.object ? expr.object->span : expr.span, "cannot access fields through iso without enter");
    return invalidType();
  }
  if (isGenericTypeName(object.name)) {
    report(expr.span, fmt::format("cannot access field '{}' on generic type '{}'", expr.fieldName, object.name));
    return invalidType();
  }
  const auto structIt = structs_.find(object.name);
  if (structIt == structs_.end()) {
    report(expr.span, fmt::format("type '{}' has no fields", object.name));
    return invalidType();
  }
  const auto fieldIt = structIt->second.fields.find(expr.fieldName);
  if (fieldIt == structIt->second.fields.end()) {
    report(expr.span, fmt::format("type '{}' has no field '{}'", object.name, expr.fieldName));
    return invalidType();
  }
  return fieldIt->second.type;
}

Type Sema::checkFieldAssign(const ast::FieldAccessExpr& target, const Type& value, Span valueSpan) {
  Type object = target.object ? checkExpr(*target.object, ValueContext::Operand) : invalidType();
  if (isInvalid(object)) {
    return invalidType();
  }
  if (object.capability != Capability::Mut) {
    report(target.object ? target.object->span : target.span, "field assignment requires a mut receiver");
    return invalidType();
  }
  const auto structIt = structs_.find(object.name);
  if (structIt == structs_.end()) {
    report(target.span, fmt::format("type '{}' has no fields", object.name));
    return invalidType();
  }
  const auto fieldIt = structIt->second.fields.find(target.fieldName);
  if (fieldIt == structIt->second.fields.end()) {
    report(target.span, fmt::format("type '{}' has no field '{}'", object.name, target.fieldName));
    return invalidType();
  }
  if (!fieldIt->second.isVar) {
    report(target.span, fmt::format("cannot assign to let field '{}'", target.fieldName));
    return invalidType();
  }
  if (!sameType(fieldIt->second.type, value)) {
    report(valueSpan, fmt::format("assignment type '{}' does not match field type '{}'", typeName(value), typeName(fieldIt->second.type)));
    return invalidType();
  }
  return unitType();
}

Type Sema::checkArrayLiteral(const ast::ArrayLiteralExpr& expr) {
  if (expr.elements.empty()) {
    report(expr.span, "cannot infer empty array literal type");
    return invalidType();
  }
  Type element = expr.elements.front() ? checkExpr(*expr.elements.front()) : invalidType();
  if (!isPlainI64(element)) {
    report(expr.elements.front() ? expr.elements.front()->span : expr.span, "array literals currently require i64 elements");
  }
  for (std::size_t i = 1; i < expr.elements.size(); ++i) {
    Type current = expr.elements[i] ? checkExpr(*expr.elements[i]) : invalidType();
    if (!sameType(element, current)) {
      report(expr.elements[i] ? expr.elements[i]->span : expr.span, "array elements must have matching type");
    }
  }
  return arrayType(Capability::None, std::move(element), expr.elements.size());
}

Type Sema::checkIndex(const ast::IndexExpr& expr) {
  Type object = expr.object ? checkExpr(*expr.object, ValueContext::Operand) : invalidType();
  Type index = expr.index ? checkExpr(*expr.index) : invalidType();
  if (!isInvalid(index) && !isIntegerType(index)) {
    report(expr.index ? expr.index->span : expr.span, "array index must be an integer");
  }
  if (isInvalid(object)) {
    return invalidType();
  }
  if (!object.isArray || object.elementType == nullptr) {
    report(expr.object ? expr.object->span : expr.span, "indexing requires an array");
    return invalidType();
  }
  return *object.elementType;
}

Type Sema::checkIndexAssign(const ast::IndexExpr& target, const Type& value, Span valueSpan) {
  Type object = target.object ? checkExpr(*target.object, ValueContext::Operand) : invalidType();
  Type index = target.index ? checkExpr(*target.index) : invalidType();
  if (!isInvalid(index) && !isIntegerType(index)) {
    report(target.index ? target.index->span : target.span, "array index must be an integer");
  }
  if (isInvalid(object)) {
    return invalidType();
  }
  if (!object.isArray || object.elementType == nullptr) {
    report(target.object ? target.object->span : target.span, "index assignment requires an array");
    return invalidType();
  }
  if (object.capability != Capability::Mut) {
    report(target.object ? target.object->span : target.span, "array index assignment requires a mut array");
    return invalidType();
  }
  if (!sameType(*object.elementType, value)) {
    report(valueSpan, fmt::format("assignment type '{}' does not match array element type '{}'", typeName(value), typeName(*object.elementType)));
    return invalidType();
  }
  return unitType();
}

Type Sema::checkBinary(const ast::BinaryExpr& expr) {
  Type lhs = expr.lhs ? checkExpr(*expr.lhs) : invalidType();
  Type rhs = expr.rhs ? checkExpr(*expr.rhs) : invalidType();
  if (isInvalid(lhs) || isInvalid(rhs)) {
    return invalidType();
  }
  if (expr.op == "+" || expr.op == "-" || expr.op == "*" || expr.op == "/") {
    if (!sameType(lhs, rhs) || !isNumericType(lhs)) {
      report(expr.span, fmt::format("operator '{}' requires matching numeric operands", expr.op));
      return invalidType();
    }
    return lhs;
  }
  if (expr.op == "%") {
    if (!sameType(lhs, rhs) || !isIntegerType(lhs)) {
      report(expr.span, "operator '%' requires matching integer operands");
      return invalidType();
    }
    return lhs;
  }
  if (expr.op == "==" || expr.op == "!=") {
    if (!sameType(lhs, rhs)) {
      report(expr.span, fmt::format("operator '{}' requires matching operand types", expr.op));
      return invalidType();
    }
    return boolType();
  }
  if (expr.op == "<" || expr.op == "<=" || expr.op == ">" || expr.op == ">=") {
    if (!sameType(lhs, rhs) || !isNumericType(lhs)) {
      report(expr.span, fmt::format("operator '{}' requires matching numeric operands", expr.op));
      return invalidType();
    }
    return boolType();
  }
  report(expr.span, fmt::format("unsupported binary operator '{}'", expr.op));
  return invalidType();
}

std::optional<std::string> Sema::resolveFunctionName(const ast::Path& path, const ast::Decl* contextDecl, Span span) {
  if (path.segments.empty()) {
    return std::nullopt;
  }
  if (path.segments.size() == 1) {
    const std::string& name = path.segments.front();
    if (contextDecl != nullptr && !contextDecl->modulePath.segments.empty()) {
      const std::string localKey = qualifiedName(contextDecl->modulePath, name);
      if (functions_.contains(localKey)) {
        return localKey;
      }
      std::vector<std::string> importedMatches;
      for (const auto& import : contextDecl->imports) {
        if (import.alias.has_value()) {
          continue;
        }
        const std::string importedKey = qualifiedName(import.path, name);
        if (functions_.contains(importedKey)) {
          importedMatches.push_back(importedKey);
        }
      }
      if (importedMatches.size() == 1) {
        return importedMatches.front();
      }
      if (importedMatches.size() > 1) {
        report(span, fmt::format("function '{}' is ambiguous between imports", name));
        return std::nullopt;
      }
    }
    auto simpleIt = simpleFunctionKeys_.find(name);
    if (simpleIt == simpleFunctionKeys_.end() || simpleIt->second.empty()) {
      return std::nullopt;
    }
    if (simpleIt->second.size() == 1) {
      return simpleIt->second.front();
    }
    report(span, fmt::format("function '{}' is ambiguous; use a qualified name", name));
    return std::nullopt;
  }

  if (contextDecl != nullptr) {
    for (const auto& import : contextDecl->imports) {
      if (!import.alias.has_value() || *import.alias != path.segments.front()) {
        continue;
      }
      ast::Path resolved = import.path;
      resolved.segments.insert(resolved.segments.end(), path.segments.begin() + 1, path.segments.end());
      const std::string key = joinPath(resolved.segments);
      if (functions_.contains(key)) {
        return key;
      }
      return std::nullopt;
    }
  }

  const std::string key = joinPath(path.segments);
  if (functions_.contains(key)) {
    return key;
  }
  auto qualifiedIt = qualifiedFunctionKeys_.find(key);
  if (qualifiedIt != qualifiedFunctionKeys_.end()) {
    return qualifiedIt->second;
  }
  return std::nullopt;
}

Type Sema::checkResolvedFunctionCall(const ast::CallExpr& expr, const FunctionSignature& signature, std::string_view diagnosticName, Span calleeSpan) {
  if (!signature.genericParams.empty()) {
    report(calleeSpan, fmt::format("calling generic function '{}' is not supported yet", diagnosticName));
    for (const auto& arg : expr.arguments) {
      if (arg != nullptr) {
        checkExpr(*arg);
      }
    }
    return invalidType();
  }
  if (expr.arguments.size() != signature.params.size()) {
    report(expr.span, fmt::format("function '{}' expects {} argument(s) but got {}", diagnosticName, signature.params.size(), expr.arguments.size()));
  }
  const std::size_t checkedCount = std::min(expr.arguments.size(), signature.params.size());
  for (std::size_t i = 0; i < checkedCount; ++i) {
    if (expr.arguments[i] == nullptr) {
      continue;
    }
    Type actual = checkExpr(*expr.arguments[i]);
    const Type& expected = signature.params[i];
    if (!sameType(expected, actual)) {
      report(expr.arguments[i]->span, fmt::format("argument {} type '{}' does not match expected type '{}'", i + 1, typeName(actual), typeName(expected)));
    }
  }
  for (std::size_t i = checkedCount; i < expr.arguments.size(); ++i) {
    if (expr.arguments[i] != nullptr) {
      checkExpr(*expr.arguments[i]);
    }
  }
  return signature.returnType;
}

Type Sema::checkFunctionCall(const ast::CallExpr& expr, const std::string& functionName, Span calleeSpan) {
  ast::Path path{{functionName}};
  auto resolved = resolveFunctionName(path, currentDecl_, calleeSpan);
  if (!resolved.has_value()) {
    report(calleeSpan, fmt::format("undefined function '{}'", functionName));
    for (const auto& arg : expr.arguments) {
      if (arg != nullptr) {
        checkExpr(*arg);
      }
    }
    return invalidType();
  }
  const auto signature = functions_.find(*resolved);
  if (signature == functions_.end()) {
    return invalidType();
  }
  return checkResolvedFunctionCall(expr, signature->second, functionName, calleeSpan);
}

Type Sema::checkCall(const ast::CallExpr& expr) {
  if (expr.callee != nullptr) {
    if (auto builtinName = qualifiedCoreBuiltinName(*expr.callee)) {
      return checkFunctionCall(expr, *builtinName, expr.callee->span);
    }
    ast::Path path = pathFromFieldAccess(*expr.callee);
    if (path.segments.size() > 1) {
      if (auto resolved = resolveFunctionName(path, currentDecl_, expr.callee->span)) {
        const auto signature = functions_.find(*resolved);
        if (signature != functions_.end()) {
          return checkResolvedFunctionCall(expr, signature->second, joinPath(path.segments), expr.callee->span);
        }
      }
    }
  }
  if (const auto* callee = dynamic_cast<const ast::NameExpr*>(expr.callee.get())) {
    return checkFunctionCall(expr, callee->name, callee->span);
  }
  if (const auto* method = dynamic_cast<const ast::FieldAccessExpr*>(expr.callee.get())) {
    return checkMethodCall(expr, *method);
  }
  report(expr.span, "function call currently requires a function name");
  for (const auto& arg : expr.arguments) {
    if (arg != nullptr) {
      checkExpr(*arg);
    }
  }
  return invalidType();
}

Type Sema::checkMethodCall(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee) {
  const auto* typeNameExpr = dynamic_cast<const ast::NameExpr*>(callee.object.get());
  const bool isTypeLevelCall = typeNameExpr != nullptr && structs_.contains(typeNameExpr->name) && lookup(typeNameExpr->name) == nullptr;
  Type receiver = isTypeLevelCall ? Type{Capability::None, typeNameExpr->name} : (callee.object ? checkExpr(*callee.object, ValueContext::Operand) : invalidType());
  if (isInvalid(receiver)) {
    for (const auto& arg : expr.arguments) {
      if (arg != nullptr) {
        checkExpr(*arg);
      }
    }
    return invalidType();
  }
  if (!isTypeLevelCall && isGenericTypeName(receiver.name)) {
    return checkGenericMethodCall(expr, callee, receiver);
  }
  const auto structIt = structs_.find(receiver.name);
  if (structIt == structs_.end()) {
    report(callee.span, fmt::format("type '{}' has no methods", receiver.name));
    return invalidType();
  }
  const auto methodIt = structIt->second.methods.find(callee.fieldName);
  if (methodIt == structIt->second.methods.end()) {
    report(callee.span, fmt::format("type '{}' has no method '{}'", receiver.name, callee.fieldName));
    for (const auto& arg : expr.arguments) {
      if (arg != nullptr) {
        checkExpr(*arg);
      }
    }
    return invalidType();
  }
  const MethodInfo& method = methodIt->second;
  if (isTypeLevelCall && method.hasReceiver) {
    report(callee.span, fmt::format("method '{}' requires a receiver", callee.fieldName));
  }
  if (!isTypeLevelCall && !method.hasReceiver) {
    report(callee.span, fmt::format("associated function '{}' must be called on type '{}'", callee.fieldName, receiver.name));
  }
  if (method.hasReceiver && !sameType(method.receiverType, receiver)) {
    report(callee.object ? callee.object->span : callee.span, fmt::format("method '{}' requires receiver type '{}'", callee.fieldName, method.receiverType.name));
  }
  if (expr.arguments.size() != method.params.size()) {
    report(expr.span, fmt::format("method '{}' expects {} argument(s) but got {}", callee.fieldName, method.params.size(), expr.arguments.size()));
  }
  const std::size_t checkedCount = std::min(expr.arguments.size(), method.params.size());
  for (std::size_t i = 0; i < checkedCount; ++i) {
    if (expr.arguments[i] == nullptr) {
      continue;
    }
    Type actual = checkExpr(*expr.arguments[i]);
    const Type& expected = method.params[i];
    if (!sameType(expected, actual)) {
      report(expr.arguments[i]->span, fmt::format("argument {} type '{}' does not match expected type '{}'", i + 1, typeName(actual), typeName(expected)));
    }
  }
  for (std::size_t i = checkedCount; i < expr.arguments.size(); ++i) {
    if (expr.arguments[i] != nullptr) {
      checkExpr(*expr.arguments[i]);
    }
  }
  return method.returnType;
}

bool Sema::methodArgumentsMatch(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee, const MethodInfo& method) {
  bool ok = true;
  if (expr.arguments.size() != method.params.size()) {
    report(expr.span, fmt::format("method '{}' expects {} argument(s) but got {}", callee.fieldName, method.params.size(), expr.arguments.size()));
    ok = false;
  }
  const std::size_t checkedCount = std::min(expr.arguments.size(), method.params.size());
  for (std::size_t i = 0; i < checkedCount; ++i) {
    if (expr.arguments[i] == nullptr) {
      continue;
    }
    Type actual = checkExpr(*expr.arguments[i]);
    const Type& expected = method.params[i];
    if (!sameType(expected, actual)) {
      report(expr.arguments[i]->span, fmt::format("argument {} type '{}' does not match expected type '{}'", i + 1, typeName(actual), typeName(expected)));
      ok = false;
    }
  }
  for (std::size_t i = checkedCount; i < expr.arguments.size(); ++i) {
    if (expr.arguments[i] != nullptr) {
      checkExpr(*expr.arguments[i]);
    }
  }
  return ok;
}

Type Sema::checkGenericMethodCall(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee, const Type& receiver) {
  const auto* generic = lookupGenericParam(receiver.name);
  if (generic == nullptr) {
    return invalidType();
  }
  std::vector<const MethodInfo*> candidates;
  for (const auto& traitName : generic->bounds) {
    const auto traitIt = traits_.find(traitName);
    if (traitIt == traits_.end()) {
      continue;
    }
    const auto methodIt = traitIt->second.methods.find(callee.fieldName);
    if (methodIt != traitIt->second.methods.end()) {
      candidates.push_back(&methodIt->second);
    }
  }
  if (candidates.empty()) {
    report(callee.span, fmt::format("generic type '{}' has no method '{}' in its trait bounds", receiver.name, callee.fieldName));
    for (const auto& arg : expr.arguments) {
      if (arg != nullptr) {
        checkExpr(*arg);
      }
    }
    return invalidType();
  }
  const MethodInfo* method = candidates.front();
  for (std::size_t i = 1; i < candidates.size(); ++i) {
    if (!sameMethodSignature(*method, *candidates[i])) {
      report(callee.span, fmt::format("method '{}' is ambiguous for generic type '{}'", callee.fieldName, receiver.name));
      return invalidType();
    }
  }
  if (method->receiverType.capability != receiver.capability) {
    report(callee.object ? callee.object->span : callee.span, fmt::format("method '{}' requires a compatible receiver", callee.fieldName));
  }
  methodArgumentsMatch(expr, callee, *method);
  return method->returnType;
}

Type Sema::checkReturn(const ast::ReturnExpr& expr) {
  Type actual = expr.value ? checkExpr(*expr.value) : unitType();
  if (regionDepth_ > 0 && isRegionLocal(actual.capability)) {
    report(expr.span, "region-local value cannot escape its region");
  }
  if (!sameType(currentReturnType_, actual)) {
    report(expr.span, fmt::format("return type '{}' does not match function return type '{}'", actual.name, currentReturnType_.name));
  }
  return actual;
}

Type Sema::checkIf(const ast::IfExpr& expr) {
  Type condition = expr.condition ? checkExpr(*expr.condition) : invalidType();
  if (!isInvalid(condition) && (condition.capability != Capability::None || condition.name != "bool")) {
    report(expr.condition ? expr.condition->span : expr.span, "if condition must be bool");
  }
  Type thenType = expr.thenBlock ? checkBlock(*expr.thenBlock) : unitType();
  if (expr.elseBranch == nullptr) {
    return unitType();
  }
  Type elseType = checkExpr(*expr.elseBranch);
  if (!sameType(thenType, elseType)) {
    report(expr.span, fmt::format("if branch type '{}' does not match else branch type '{}'", thenType.name, elseType.name));
    return invalidType();
  }
  return thenType;
}

Type Sema::checkWhile(const ast::WhileExpr& expr) {
  Type condition = expr.condition ? checkExpr(*expr.condition) : invalidType();
  if (!isInvalid(condition) && (condition.capability != Capability::None || condition.name != "bool")) {
    report(expr.condition ? expr.condition->span : expr.span, "while condition must be bool");
  }
  if (expr.body != nullptr) {
    checkBlock(*expr.body);
  }
  return unitType();
}

Type Sema::checkRegion(const ast::RegionExpr& expr) {
  const auto* sourceName = asNameExpr(expr.source.get());
  BindingState* sourceBinding = sourceName != nullptr ? lookup(sourceName->name) : nullptr;
  Type source = sourceName != nullptr ? checkName(*sourceName, ValueContext::Operand) : invalidType();
  if (sourceName == nullptr || sourceBinding == nullptr || source.capability != Capability::Iso) {
    report(expr.source ? expr.source->span : expr.span, "enter/explore source must be an iso binding");
  }

  bool previousMoved = false;
  if (sourceBinding != nullptr) {
    previousMoved = sourceBinding->moved;
    if (!sourceBinding->moved) {
      sourceBinding->moved = true;
    }
  }

  ++regionDepth_;
  pushScope();
  if (!expr.bindingName.empty() && !isInvalid(source)) {
    const Capability bindingCapability = expr.kind == ast::RegionExpr::Kind::Enter ? Capability::Mut : Capability::Paused;
    declare(expr.bindingName, BindingState{Type{bindingCapability, source.name}, bindingCapability == Capability::Mut, false, expr.span, BindingOrigin::RegionBridge, regionDepth_, true});
  }
  Type result = expr.body ? checkBlock(*expr.body) : unitType();
  popScope();
  --regionDepth_;

  if (sourceBinding != nullptr) {
    sourceBinding->moved = previousMoved;
  }

  if (isRegionLocal(result.capability)) {
    report(expr.span, "region block cannot return mut/tmp/paused values");
  }
  return result;
}

Type Sema::checkFreeze(const ast::FreezeExpr& expr) {
  if (dynamic_cast<const ast::MoveExpr*>(expr.value.get()) == nullptr) {
    report(expr.value ? expr.value->span : expr.span, "freeze requires move of an iso value");
    return invalidType();
  }
  Type input = checkExpr(*expr.value);
  if (input.capability != Capability::Iso) {
    report(expr.value ? expr.value->span : expr.span, "freeze requires an iso value");
    return invalidType();
  }
  return Type{Capability::Imm, input.name};
}

Type Sema::checkMerge(const ast::MergeExpr& expr) {
  if (dynamic_cast<const ast::MoveExpr*>(expr.value.get()) == nullptr) {
    report(expr.value ? expr.value->span : expr.span, "merge requires move of an iso value");
    return invalidType();
  }
  if (regionDepth_ == 0) {
    report(expr.span, "merge requires an active region");
  }
  Type input = checkExpr(*expr.value);
  if (input.capability != Capability::Iso) {
    report(expr.value ? expr.value->span : expr.span, "merge requires an iso value");
    return invalidType();
  }
  return Type{Capability::Mut, input.name};
}

Type Sema::typeFromAst(const ast::TypeRef& type) {
  if (type.isArray) {
    Type element = type.elementType ? typeFromAst(*type.elementType) : invalidType();
    if (element.isArray) {
      report(type.elementType ? type.elementType->span : type.span, "nested arrays are not supported yet");
    }
    if (!isPlainI64(element)) {
      report(type.elementType ? type.elementType->span : type.span, "arrays currently support only i64 elements");
    }
    return arrayType(capabilityFromAst(type.capability), std::move(element));
  }
  if (type.path.size() > 1) {
    return Type{capabilityFromAst(type.capability), joinPath(type.path)};
  }
  return Type{capabilityFromAst(type.capability), type.name};
}

Capability Sema::capabilityFromAst(ast::Capability capability) const {
  switch (capability) {
  case ast::Capability::Mut: return Capability::Mut;
  case ast::Capability::Tmp: return Capability::Tmp;
  case ast::Capability::Iso: return Capability::Iso;
  case ast::Capability::Imm: return Capability::Imm;
  case ast::Capability::Paused: return Capability::Paused;
  case ast::Capability::Cown: return Capability::Cown;
  case ast::Capability::None: return Capability::None;
  }
  return Capability::None;
}

bool Sema::isRegionLocal(Capability capability) const {
  return capability == Capability::Mut || capability == Capability::Tmp || capability == Capability::Paused;
}

bool Sema::isRegionLocalBinding(const BindingState& binding) const {
  return binding.origin == BindingOrigin::RegionBridge || binding.origin == BindingOrigin::RegionLocal || isRegionLocal(binding.type.capability);
}

bool Sema::sameType(const Type& lhs, const Type& rhs) const {
  if (isInvalid(lhs) || isInvalid(rhs)) {
    return true;
  }
  if (lhs.capability != rhs.capability) {
    return false;
  }
  if (lhs.isArray || rhs.isArray) {
    if (!lhs.isArray || !rhs.isArray || lhs.elementType == nullptr || rhs.elementType == nullptr) {
      return false;
    }
    if (!sameType(*lhs.elementType, *rhs.elementType)) {
      return false;
    }
    return !lhs.arrayLength.has_value() || !rhs.arrayLength.has_value() || lhs.arrayLength == rhs.arrayLength;
  }
  return lhs.name == rhs.name;
}

bool Sema::sameMethodSignature(const MethodInfo& expected, const MethodInfo& actual) const {
  if (expected.hasReceiver != actual.hasReceiver) {
    return false;
  }
  if (expected.hasReceiver && expected.receiverType.capability != actual.receiverType.capability) {
    return false;
  }
  if (expected.params.size() != actual.params.size()) {
    return false;
  }
  for (std::size_t i = 0; i < expected.params.size(); ++i) {
    if (!sameType(expected.params[i], actual.params[i])) {
      return false;
    }
  }
  return sameType(expected.returnType, actual.returnType);
}

void Sema::report(Span span, std::string message) {
  diagnostics_.error(span, std::move(message));
}

} // namespace gura::hir
