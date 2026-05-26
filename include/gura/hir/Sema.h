#pragma once

#include "gura/ast/Decl.h"
#include "gura/basic/Diagnostic.h"
#include "gura/hir/Type.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gura::hir {

class Sema {
public:
  explicit Sema(DiagnosticEngine& diagnostics) : diagnostics_(diagnostics) {}
  bool check(const ast::SourceFile& file);

private:
  enum class ValueContext {
    Ordinary,
    Operand,
  };

  enum class BindingOrigin {
    Ordinary,
    RegionBridge,
    RegionLocal,
  };

  struct BindingState {
    Type type;
    bool isVar = false;
    bool moved = false;
    Span span;
    BindingOrigin origin = BindingOrigin::Ordinary;
    int regionDepth = 0;
    bool bridgeSlot = false;
    bool bridgeReassigned = false;
  };

  struct FunctionSignature {
    std::vector<Type> params;
    Type returnType;
    std::vector<ast::GenericParam> genericParams;
    Span span;
  };

  struct FieldInfo {
    Type type;
    bool isVar = false;
    Span span;
  };

  struct MethodInfo {
    std::vector<Type> params;
    Type receiverType;
    Type returnType;
    bool hasReceiver = false;
    const ast::FnDecl* decl = nullptr;
    Span span;
  };

  struct StructInfo {
    std::unordered_map<std::string, FieldInfo> fields;
    std::unordered_map<std::string, MethodInfo> methods;
    Span span;
  };

  struct TraitInfo {
    std::unordered_map<std::string, MethodInfo> methods;
    Span span;
  };

  struct GenericParamInfo {
    std::vector<std::string> bounds;
    Span span;
  };

  using GenericEnv = std::unordered_map<std::string, GenericParamInfo>;

  void pushScope();
  void popScope();
  void declare(std::string name, BindingState state);
  BindingState* lookup(const std::string& name);

  void collectStructs(const ast::SourceFile& file);
  void collectTraits(const ast::SourceFile& file);
  void collectImplMethods(const ast::SourceFile& file);
  void collectFunctionSignatures(const ast::SourceFile& file);
  std::optional<MethodInfo> makeMethodInfo(const ast::FnDecl& method, const std::string& selfTypeName, bool requireReceiver);
  void collectMethod(StructInfo& info, const ast::FnDecl& method, const std::string& selfTypeName, bool requireReceiver);
  void collectTraitMethod(TraitInfo& info, const ast::FnDecl& method, const std::string& traitName);
  void validateTraitImpls(const ast::SourceFile& file);
  bool sameMethodSignature(const MethodInfo& expected, const MethodInfo& actual) const;
  GenericEnv makeGenericEnv(const ast::FnDecl& fn);
  bool isGenericTypeName(const std::string& name) const;
  const GenericParamInfo* lookupGenericParam(const std::string& name) const;
  bool isBuiltinTypeName(const std::string& name) const;
  bool methodArgumentsMatch(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee, const MethodInfo& method);
  Type checkGenericMethodCall(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee, const Type& receiver);
  bool checkDecl(const ast::Decl& decl);
  bool checkFn(const ast::FnDecl& fn);
  bool checkMethod(const ast::FnDecl& fn, const std::string& selfTypeName);
  Type checkBlock(const ast::BlockExpr& block);
  Type checkExpr(const ast::Expr& expr, ValueContext context = ValueContext::Ordinary);
  Type checkName(const ast::NameExpr& expr, ValueContext context);
  Type checkMove(const ast::MoveExpr& expr);
  Type checkNew(const ast::NewExpr& expr);
  Type checkBinding(const ast::BindingExpr& expr);
  Type checkAssign(const ast::AssignExpr& expr);
  Type checkFieldAccess(const ast::FieldAccessExpr& expr);
  Type checkFieldAssign(const ast::FieldAccessExpr& target, const Type& value, Span valueSpan);
  Type checkArrayLiteral(const ast::ArrayLiteralExpr& expr);
  Type checkIndex(const ast::IndexExpr& expr);
  Type checkIndexAssign(const ast::IndexExpr& target, const Type& value, Span valueSpan);
  Type checkBinary(const ast::BinaryExpr& expr);
  Type checkCall(const ast::CallExpr& expr);
  Type checkMethodCall(const ast::CallExpr& expr, const ast::FieldAccessExpr& callee);
  Type checkReturn(const ast::ReturnExpr& expr);
  Type checkIf(const ast::IfExpr& expr);
  Type checkWhile(const ast::WhileExpr& expr);
  Type checkRegion(const ast::RegionExpr& expr);
  Type checkFreeze(const ast::FreezeExpr& expr);
  Type checkMerge(const ast::MergeExpr& expr);

  [[nodiscard]] Type typeFromAst(const ast::TypeRef& type);
  [[nodiscard]] Capability capabilityFromAst(ast::Capability capability) const;
  [[nodiscard]] bool isRegionLocal(Capability capability) const;
  [[nodiscard]] bool isRegionLocalBinding(const BindingState& binding) const;
  [[nodiscard]] bool sameType(const Type& lhs, const Type& rhs) const;
  void report(Span span, std::string message);

  DiagnosticEngine& diagnostics_;
  std::vector<std::unordered_map<std::string, BindingState>> scopes_;
  std::unordered_map<std::string, StructInfo> structs_;
  std::unordered_map<std::string, TraitInfo> traits_;
  std::unordered_map<std::string, FunctionSignature> functions_;
  GenericEnv currentGenericParams_;
  Type currentReturnType_{Capability::None, "unit"};
  int regionDepth_ = 0;
};

} // namespace gura::hir
