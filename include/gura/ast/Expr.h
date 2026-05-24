#pragma once

#include "gura/ast/Node.h"
#include "gura/ast/TypeRef.h"

#include <string>

namespace gura::ast {

struct Expr : Node {};
struct BlockExpr;

enum class LiteralKind {
  Integer,
  Float,
  Bool,
  None,
  String,
  Char,
};

struct LiteralExpr : Expr {
  LiteralKind kind = LiteralKind::Integer;
  std::string value;
  std::string suffix;
};

struct NameExpr : Expr {
  std::string name;
};

struct BinaryExpr : Expr {
  std::string op;
  Ptr<Expr> lhs;
  Ptr<Expr> rhs;
};

struct AssignExpr : Expr {
  Ptr<Expr> target;
  Ptr<Expr> value;
};

struct FieldAccessExpr : Expr {
  Ptr<Expr> object;
  std::string fieldName;
};

struct BindingExpr : Expr {
  bool isVar = false;
  std::string name;
  Ptr<TypeRef> type;
  Ptr<Expr> initializer;
};

struct MoveExpr : Expr {
  Ptr<Expr> value;
};

struct FieldInit : Node {
  std::string name;
  Ptr<Expr> value;
};

struct NewExpr : Expr {
  Ptr<TypeRef> type;
  std::vector<FieldInit> fields;
};

struct CallExpr : Expr {
  Ptr<Expr> callee;
  List<Expr> arguments;
};

struct FreezeExpr : Expr {
  Ptr<Expr> value;
};

struct MergeExpr : Expr {
  Ptr<Expr> value;
};

struct IfExpr : Expr {
  Ptr<Expr> condition;
  Ptr<BlockExpr> thenBlock;
  Ptr<Expr> elseBranch;
};

struct WhileExpr : Expr {
  Ptr<Expr> condition;
  Ptr<BlockExpr> body;
};

struct BlockExpr : Expr {
  List<Expr> expressions;
};

struct RegionExpr : Expr {
  enum class Kind {
    Enter,
    Explore,
  };

  Kind kind = Kind::Enter;
  Ptr<Expr> source;
  std::string bindingName;
  Ptr<BlockExpr> body;
};

struct ReturnExpr : Expr {
  Ptr<Expr> value;
};

} // namespace gura::ast
