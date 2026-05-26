#pragma once

#include "gura/ast/Decl.h"
#include "gura/basic/Diagnostic.h"
#include "gura/lexer/Token.h"

#include <span>
#include <vector>

namespace gura {

class Parser {
public:
  Parser(std::span<const Token> tokens, DiagnosticEngine& diagnostics);

  ast::Ptr<ast::SourceFile> parseSourceFile();

private:
  [[nodiscard]] const Token& peek(std::size_t lookahead = 0) const;
  [[nodiscard]] bool at(TokenKind kind) const;
  bool consume(TokenKind kind);
  const Token& advance();
  void expect(TokenKind kind, std::string message);

  ast::Ptr<ast::Decl> parseDecl();
  ast::Ptr<ast::FnDecl> parseFnDecl(bool requireBody = true);
  ast::Ptr<ast::StructDecl> parseStructDecl();
  ast::Ptr<ast::TraitDecl> parseTraitDecl();
  ast::Ptr<ast::ImplDecl> parseImplDecl();
  std::vector<ast::GenericParam> parseGenericParams();
  ast::GenericParam parseGenericParam();
  ast::FieldDecl parseFieldDecl();
  ast::Ptr<ast::TypeRef> parseTypeRef();
  ast::Ptr<ast::BlockExpr> parseBlock();
  ast::Ptr<ast::Expr> parseExpr();
  ast::Ptr<ast::Expr> parseAssignExpr();
  ast::Ptr<ast::Expr> parseBinaryExpr(int minPrecedence = 1);
  ast::Ptr<ast::Expr> parseReturnExpr();
  ast::Ptr<ast::Expr> parseBindingExpr();
  ast::Ptr<ast::Expr> parsePostfixExpr();
  ast::Ptr<ast::Expr> parsePrimaryExpr();
  ast::Ptr<ast::Expr> parseMoveExpr();
  ast::Ptr<ast::Expr> parseNewExpr();
  ast::FieldInit parseFieldInit();
  ast::Ptr<ast::Expr> parseFreezeExpr();
  ast::Ptr<ast::Expr> parseMergeExpr();
  ast::Ptr<ast::Expr> parseIfExpr();
  ast::Ptr<ast::Expr> parseWhileExpr();
  ast::Ptr<ast::Expr> parseRegionExpr(ast::RegionExpr::Kind kind);

  ast::List<ast::Expr> parseCallArguments();
  ast::NodeId nextNodeId();

  std::span<const Token> tokens_;
  DiagnosticEngine& diagnostics_;
  std::size_t index_ = 0;
  ast::NodeId nextNodeId_ = 1;
};

} // namespace gura
