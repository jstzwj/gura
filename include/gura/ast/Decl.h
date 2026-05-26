#pragma once

#include "gura/ast/Expr.h"
#include "gura/ast/TypeRef.h"

#include <optional>
#include <string>
#include <vector>

namespace gura::ast {

struct Decl : Node {};

struct Param {
  std::string name;
  Ptr<TypeRef> type;
};

struct TraitBound {
  std::string name;
  Span span;
};

struct GenericParam {
  std::string name;
  std::vector<TraitBound> bounds;
  Span span;
};

struct FnDecl : Decl {
  std::string name;
  std::vector<GenericParam> genericParams;
  std::vector<Param> params;
  Ptr<TypeRef> returnType;
  Ptr<BlockExpr> body;
};

struct FieldDecl : Node {
  bool isVar = false;
  std::string name;
  Ptr<TypeRef> type;
};

struct StructDecl : Decl {
  std::string name;
  std::vector<FieldDecl> fields;
  std::vector<Ptr<FnDecl>> methods;
};

struct TraitDecl : Decl {
  std::string name;
  std::vector<Ptr<FnDecl>> methods;
};

struct ImplDecl : Decl {
  std::string typeName;
  std::optional<std::string> traitName;
  std::vector<Ptr<FnDecl>> methods;
};

struct SourceFile : Node {
  List<Decl> declarations;
};

} // namespace gura::ast
