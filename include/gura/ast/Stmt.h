#pragma once

#include "gura/ast/Expr.h"
#include "gura/ast/TypeRef.h"

#include <string>

namespace gura::ast {

struct Stmt : Node {};

struct LetStmt : Stmt {
  bool isVar = false;
  std::string name;
  Ptr<TypeRef> type;
  Ptr<Expr> value;
};

} // namespace gura::ast
