#pragma once

#include "gura/ast/Expr.h"
#include "gura/ast/TypeRef.h"

#include <string>

namespace gura::ast {

struct Decl : Node {};

struct Param {
  std::string name;
  Ptr<TypeRef> type;
};

struct FnDecl : Decl {
  std::string name;
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

struct ImplDecl : Decl {
  std::string typeName;
  std::vector<Ptr<FnDecl>> methods;
};

struct SourceFile : Node {
  List<Decl> declarations;
};

} // namespace gura::ast
