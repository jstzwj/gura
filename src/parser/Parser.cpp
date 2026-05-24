#include "gura/parser/Parser.h"

#include <string_view>
#include <utility>

namespace gura {

namespace {

std::string numericSuffix(std::string_view text, ast::LiteralKind kind) {
  if (kind == ast::LiteralKind::Integer) {
    if (text.ends_with("i32") || text.ends_with("i64")) {
      return std::string(text.substr(text.size() - 3));
    }
    return {};
  }
  if (kind == ast::LiteralKind::Float) {
    if (text.ends_with("f32") || text.ends_with("f64")) {
      return std::string(text.substr(text.size() - 3));
    }
  }
  return {};
}

int binaryPrecedence(TokenKind kind) {
  switch (kind) {
  case TokenKind::EqualEqual:
  case TokenKind::BangEqual:
  case TokenKind::Less:
  case TokenKind::LessEqual:
  case TokenKind::Greater:
  case TokenKind::GreaterEqual: return 1;
  case TokenKind::Plus:
  case TokenKind::Minus: return 2;
  case TokenKind::Star:
  case TokenKind::Slash:
  case TokenKind::Percent: return 3;
  default: return 0;
  }
}

} // namespace

Parser::Parser(std::span<const Token> tokens, DiagnosticEngine& diagnostics) : tokens_(tokens), diagnostics_(diagnostics) {}

ast::Ptr<ast::SourceFile> Parser::parseSourceFile() {
  auto file = std::make_unique<ast::SourceFile>();
  file->id = nextNodeId();
  while (!at(TokenKind::EndOfFile)) {
    if (auto decl = parseDecl()) {
      file->declarations.push_back(std::move(decl));
    } else {
      advance();
    }
  }
  return file;
}

const Token& Parser::peek(std::size_t lookahead) const {
  const auto idx = index_ + lookahead;
  if (idx >= tokens_.size()) {
    return tokens_.back();
  }
  return tokens_[idx];
}

bool Parser::at(TokenKind kind) const {
  return peek().kind == kind;
}

bool Parser::consume(TokenKind kind) {
  if (!at(kind)) {
    return false;
  }
  advance();
  return true;
}

const Token& Parser::advance() {
  const Token& token = peek();
  if (!at(TokenKind::EndOfFile)) {
    ++index_;
  }
  return token;
}

void Parser::expect(TokenKind kind, std::string message) {
  if (!consume(kind)) {
    diagnostics_.error(peek().span, std::move(message));
  }
}

ast::Ptr<ast::Decl> Parser::parseDecl() {
  if (at(TokenKind::KwFn)) {
    return parseFnDecl();
  }
  if (at(TokenKind::KwStruct)) {
    return parseStructDecl();
  }
  if (at(TokenKind::KwImpl)) {
    return parseImplDecl();
  }
  diagnostics_.error(peek().span, "expected declaration");
  return nullptr;
}

ast::Ptr<ast::FnDecl> Parser::parseFnDecl() {
  const auto begin = advance().span.begin;
  auto fn = std::make_unique<ast::FnDecl>();
  fn->id = nextNodeId();
  if (!at(TokenKind::Identifier) && !at(TokenKind::KwNew)) {
    diagnostics_.error(peek().span, "expected function name");
    return fn;
  }
  fn->name = advance().text;
  expect(TokenKind::LParen, "expected '(' after function name");
  if (!at(TokenKind::RParen)) {
    do {
      ast::Param param;
      if (!at(TokenKind::Identifier)) {
        diagnostics_.error(peek().span, "expected parameter name");
        break;
      }
      param.name = advance().text;
      expect(TokenKind::Colon, "expected ':' after parameter name");
      param.type = parseTypeRef();
      fn->params.push_back(std::move(param));
    } while (consume(TokenKind::Comma));
  }
  expect(TokenKind::RParen, "expected ')' after parameter list");
  if (consume(TokenKind::Colon)) {
    fn->returnType = parseTypeRef();
  }
  fn->body = parseBlock();
  fn->span = Span{begin, fn->body ? fn->body->span.end : peek().span.end};
  return fn;
}

ast::Ptr<ast::StructDecl> Parser::parseStructDecl() {
  const auto begin = advance().span.begin;
  auto decl = std::make_unique<ast::StructDecl>();
  decl->id = nextNodeId();
  if (!at(TokenKind::Identifier)) {
    diagnostics_.error(peek().span, "expected struct name");
    return decl;
  }
  decl->name = advance().text;
  expect(TokenKind::LBrace, "expected '{' after struct name");
  while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
    if (at(TokenKind::KwFn)) {
      decl->methods.push_back(parseFnDecl());
    } else {
      decl->fields.push_back(parseFieldDecl());
      consume(TokenKind::Semicolon);
    }
  }
  const auto end = peek().span.end;
  expect(TokenKind::RBrace, "expected '}' after struct body");
  decl->span = Span{begin, end};
  return decl;
}

ast::Ptr<ast::ImplDecl> Parser::parseImplDecl() {
  const auto begin = advance().span.begin;
  auto decl = std::make_unique<ast::ImplDecl>();
  decl->id = nextNodeId();
  if (!at(TokenKind::Identifier)) {
    diagnostics_.error(peek().span, "expected impl type name");
    return decl;
  }
  decl->typeName = advance().text;
  expect(TokenKind::LBrace, "expected '{' after impl type name");
  while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
    if (!at(TokenKind::KwFn)) {
      diagnostics_.error(peek().span, "expected method declaration");
      advance();
      continue;
    }
    decl->methods.push_back(parseFnDecl());
  }
  const auto end = peek().span.end;
  expect(TokenKind::RBrace, "expected '}' after impl body");
  decl->span = Span{begin, end};
  return decl;
}

ast::FieldDecl Parser::parseFieldDecl() {
  ast::FieldDecl field;
  field.id = nextNodeId();
  field.span.begin = peek().span.begin;
  if (!at(TokenKind::KwLet) && !at(TokenKind::KwVar)) {
    diagnostics_.error(peek().span, "expected field declaration");
    advance();
    return field;
  }
  field.isVar = advance().kind == TokenKind::KwVar;
  if (!at(TokenKind::Identifier)) {
    diagnostics_.error(peek().span, "expected field name");
    return field;
  }
  field.name = advance().text;
  expect(TokenKind::Colon, "expected ':' after field name");
  field.type = parseTypeRef();
  field.span.end = field.type ? field.type->span.end : peek().span.end;
  return field;
}

ast::Ptr<ast::TypeRef> Parser::parseTypeRef() {
  auto type = std::make_unique<ast::TypeRef>();
  type->id = nextNodeId();
  type->span.begin = peek().span.begin;
  switch (peek().kind) {
  case TokenKind::KwMut: type->capability = ast::Capability::Mut; advance(); break;
  case TokenKind::KwTmp: type->capability = ast::Capability::Tmp; advance(); break;
  case TokenKind::KwIso: type->capability = ast::Capability::Iso; advance(); break;
  case TokenKind::KwImm: type->capability = ast::Capability::Imm; advance(); break;
  case TokenKind::KwPau: type->capability = ast::Capability::Paused; advance(); break;
  case TokenKind::KwCown: type->capability = ast::Capability::Cown; advance(); break;
  default: break;
  }
  if (!at(TokenKind::Identifier)) {
    if (type->capability != ast::Capability::None) {
      type->span.end = peek().span.begin;
      return type;
    }
    diagnostics_.error(peek().span, "expected type name");
    return type;
  }
  type->name = advance().text;
  type->optional = consume(TokenKind::Question);
  type->span.end = peek().span.begin;
  return type;
}

ast::Ptr<ast::BlockExpr> Parser::parseBlock() {
  const auto begin = peek().span.begin;
  expect(TokenKind::LBrace, "expected block");
  auto block = std::make_unique<ast::BlockExpr>();
  block->id = nextNodeId();
  while (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
    auto expr = parseExpr();
    if (expr) {
      block->expressions.push_back(std::move(expr));
    } else if (!at(TokenKind::RBrace) && !at(TokenKind::EndOfFile)) {
      advance();
    }
    consume(TokenKind::Semicolon);
  }
  const auto end = peek().span.end;
  expect(TokenKind::RBrace, "expected '}' after block");
  block->span = Span{begin, end};
  return block;
}

ast::Ptr<ast::Expr> Parser::parseExpr() {
  if (at(TokenKind::KwLet) || at(TokenKind::KwVar)) {
    return parseBindingExpr();
  }
  if (at(TokenKind::KwReturn)) {
    return parseReturnExpr();
  }
  return parseAssignExpr();
}

ast::Ptr<ast::Expr> Parser::parseAssignExpr() {
  auto lhs = parseBinaryExpr();
  if (!consume(TokenKind::Equal)) {
    return lhs;
  }
  if (dynamic_cast<ast::NameExpr*>(lhs.get()) == nullptr && dynamic_cast<ast::FieldAccessExpr*>(lhs.get()) == nullptr) {
    diagnostics_.error(lhs ? lhs->span : peek().span, "assignment target must be a binding or field");
  }
  auto assign = std::make_unique<ast::AssignExpr>();
  assign->id = nextNodeId();
  assign->span.begin = lhs ? lhs->span.begin : peek().span.begin;
  assign->target = std::move(lhs);
  assign->value = parseAssignExpr();
  assign->span.end = assign->value ? assign->value->span.end : peek().span.end;
  return assign;
}

ast::Ptr<ast::Expr> Parser::parseBinaryExpr(int minPrecedence) {
  auto lhs = parsePostfixExpr();
  while (lhs) {
    const int precedence = binaryPrecedence(peek().kind);
    if (precedence < minPrecedence) {
      break;
    }
    const auto op = advance();
    auto rhs = parseBinaryExpr(precedence + 1);
    auto binary = std::make_unique<ast::BinaryExpr>();
    binary->id = nextNodeId();
    binary->op = op.text;
    binary->span = Span{lhs->span.begin, rhs ? rhs->span.end : op.span.end};
    binary->lhs = std::move(lhs);
    binary->rhs = std::move(rhs);
    lhs = std::move(binary);
  }
  return lhs;
}

ast::Ptr<ast::Expr> Parser::parseReturnExpr() {
  const auto begin = advance().span.begin;
  auto ret = std::make_unique<ast::ReturnExpr>();
  ret->id = nextNodeId();
  ret->value = parseExpr();
  ret->span = Span{begin, ret->value ? ret->value->span.end : peek().span.end};
  return ret;
}

ast::Ptr<ast::Expr> Parser::parseBindingExpr() {
  const auto keyword = advance();
  auto binding = std::make_unique<ast::BindingExpr>();
  binding->id = nextNodeId();
  binding->isVar = keyword.kind == TokenKind::KwVar;
  if (!at(TokenKind::Identifier)) {
    diagnostics_.error(peek().span, "expected binding name");
    return binding;
  }
  binding->name = advance().text;
  if (consume(TokenKind::Colon)) {
    binding->type = parseTypeRef();
  }
  expect(TokenKind::Equal, "expected '=' in binding");
  binding->initializer = parseExpr();
  binding->span = Span{keyword.span.begin, binding->initializer ? binding->initializer->span.end : peek().span.end};
  return binding;
}

ast::Ptr<ast::Expr> Parser::parsePostfixExpr() {
  auto expr = parsePrimaryExpr();
  while (expr && (at(TokenKind::LParen) || at(TokenKind::Dot))) {
    if (at(TokenKind::LParen)) {
      auto call = std::make_unique<ast::CallExpr>();
      call->id = nextNodeId();
      call->span.begin = expr->span.begin;
      call->callee = std::move(expr);
      call->arguments = parseCallArguments();
      call->span.end = peek().span.begin;
      expr = std::move(call);
      continue;
    }
    advance();
    auto field = std::make_unique<ast::FieldAccessExpr>();
    field->id = nextNodeId();
    field->span.begin = expr->span.begin;
    field->object = std::move(expr);
    if (!at(TokenKind::Identifier) && !at(TokenKind::KwNew)) {
      diagnostics_.error(peek().span, "expected field name after '.'");
    } else {
      field->fieldName = advance().text;
    }
    field->span.end = peek().span.begin;
    expr = std::move(field);
  }
  return expr;
}

ast::Ptr<ast::Expr> Parser::parsePrimaryExpr() {
  if (at(TokenKind::Identifier)) {
    auto name = std::make_unique<ast::NameExpr>();
    name->id = nextNodeId();
    name->span = peek().span;
    name->name = advance().text;
    return name;
  }
  if (at(TokenKind::IntegerLiteral) || at(TokenKind::FloatLiteral) || at(TokenKind::StringLiteral) || at(TokenKind::CharLiteral) || at(TokenKind::KwTrue) || at(TokenKind::KwFalse) || at(TokenKind::KwNone)) {
    auto literal = std::make_unique<ast::LiteralExpr>();
    literal->id = nextNodeId();
    literal->span = peek().span;
    const Token token = advance();
    literal->value = token.text;
    switch (token.kind) {
    case TokenKind::IntegerLiteral: literal->kind = ast::LiteralKind::Integer; break;
    case TokenKind::FloatLiteral: literal->kind = ast::LiteralKind::Float; break;
    case TokenKind::StringLiteral: literal->kind = ast::LiteralKind::String; break;
    case TokenKind::CharLiteral: literal->kind = ast::LiteralKind::Char; break;
    case TokenKind::KwNone: literal->kind = ast::LiteralKind::None; break;
    default: literal->kind = ast::LiteralKind::Bool; break;
    }
    literal->suffix = numericSuffix(literal->value, literal->kind);
    return literal;
  }
  if (at(TokenKind::LBrace)) {
    return parseBlock();
  }
  if (consume(TokenKind::LParen)) {
    auto expr = parseExpr();
    expect(TokenKind::RParen, "expected ')' after expression");
    return expr;
  }
  if (at(TokenKind::KwMove)) {
    return parseMoveExpr();
  }
  if (at(TokenKind::KwNew)) {
    return parseNewExpr();
  }
  if (at(TokenKind::KwFreeze)) {
    return parseFreezeExpr();
  }
  if (at(TokenKind::KwMerge)) {
    return parseMergeExpr();
  }
  if (at(TokenKind::KwIf)) {
    return parseIfExpr();
  }
  if (at(TokenKind::KwWhile)) {
    return parseWhileExpr();
  }
  if (at(TokenKind::KwEnter)) {
    return parseRegionExpr(ast::RegionExpr::Kind::Enter);
  }
  if (at(TokenKind::KwExplore)) {
    return parseRegionExpr(ast::RegionExpr::Kind::Explore);
  }
  diagnostics_.error(peek().span, "expected expression");
  return nullptr;
}

ast::Ptr<ast::Expr> Parser::parseMoveExpr() {
  const auto begin = advance().span.begin;
  auto move = std::make_unique<ast::MoveExpr>();
  move->id = nextNodeId();
  move->value = parsePrimaryExpr();
  move->span = Span{begin, move->value ? move->value->span.end : peek().span.end};
  return move;
}

ast::Ptr<ast::Expr> Parser::parseNewExpr() {
  const auto begin = advance().span.begin;
  auto newExpr = std::make_unique<ast::NewExpr>();
  newExpr->id = nextNodeId();
  newExpr->type = parseTypeRef();
  if (at(TokenKind::LParen)) {
    parseCallArguments();
  }
  if (consume(TokenKind::LBrace)) {
    if (!at(TokenKind::RBrace)) {
      do {
        newExpr->fields.push_back(parseFieldInit());
      } while (consume(TokenKind::Comma));
    }
    expect(TokenKind::RBrace, "expected '}' after field initializers");
  }
  newExpr->span = Span{begin, peek().span.begin};
  return newExpr;
}

ast::FieldInit Parser::parseFieldInit() {
  ast::FieldInit field;
  field.id = nextNodeId();
  field.span.begin = peek().span.begin;
  if (!at(TokenKind::Identifier)) {
    diagnostics_.error(peek().span, "expected field name in initializer");
    return field;
  }
  field.name = advance().text;
  expect(TokenKind::Colon, "expected ':' after field name");
  field.value = parseExpr();
  field.span.end = field.value ? field.value->span.end : peek().span.end;
  return field;
}

ast::Ptr<ast::Expr> Parser::parseFreezeExpr() {
  const auto begin = advance().span.begin;
  auto freeze = std::make_unique<ast::FreezeExpr>();
  freeze->id = nextNodeId();
  if (consume(TokenKind::LParen)) {
    freeze->value = parseExpr();
    expect(TokenKind::RParen, "expected ')' after freeze operand");
  } else {
    freeze->value = parsePrimaryExpr();
  }
  freeze->span = Span{begin, freeze->value ? freeze->value->span.end : peek().span.end};
  return freeze;
}

ast::Ptr<ast::Expr> Parser::parseMergeExpr() {
  const auto begin = advance().span.begin;
  auto merge = std::make_unique<ast::MergeExpr>();
  merge->id = nextNodeId();
  if (consume(TokenKind::LParen)) {
    merge->value = parseExpr();
    expect(TokenKind::RParen, "expected ')' after merge operand");
  } else {
    merge->value = parsePrimaryExpr();
  }
  merge->span = Span{begin, merge->value ? merge->value->span.end : peek().span.end};
  return merge;
}

ast::Ptr<ast::Expr> Parser::parseIfExpr() {
  const auto begin = advance().span.begin;
  auto ifExpr = std::make_unique<ast::IfExpr>();
  ifExpr->id = nextNodeId();
  ifExpr->condition = parseExpr();
  ifExpr->thenBlock = parseBlock();
  if (consume(TokenKind::KwElse)) {
    if (at(TokenKind::KwIf)) {
      ifExpr->elseBranch = parseIfExpr();
    } else {
      ifExpr->elseBranch = parseBlock();
    }
  }
  ifExpr->span = Span{begin, ifExpr->elseBranch ? ifExpr->elseBranch->span.end : (ifExpr->thenBlock ? ifExpr->thenBlock->span.end : peek().span.end)};
  return ifExpr;
}

ast::Ptr<ast::Expr> Parser::parseWhileExpr() {
  const auto begin = advance().span.begin;
  auto whileExpr = std::make_unique<ast::WhileExpr>();
  whileExpr->id = nextNodeId();
  whileExpr->condition = parseExpr();
  whileExpr->body = parseBlock();
  whileExpr->span = Span{begin, whileExpr->body ? whileExpr->body->span.end : peek().span.end};
  return whileExpr;
}

ast::Ptr<ast::Expr> Parser::parseRegionExpr(ast::RegionExpr::Kind kind) {
  const auto begin = advance().span.begin;
  auto region = std::make_unique<ast::RegionExpr>();
  region->id = nextNodeId();
  region->kind = kind;
  region->source = parseExpr();
  if (consume(TokenKind::KwAs)) {
    if (!at(TokenKind::Identifier)) {
      diagnostics_.error(peek().span, "expected region binding name");
    } else {
      region->bindingName = advance().text;
    }
  }
  region->body = parseBlock();
  region->span = Span{begin, region->body ? region->body->span.end : peek().span.end};
  return region;
}

ast::List<ast::Expr> Parser::parseCallArguments() {
  ast::List<ast::Expr> arguments;
  expect(TokenKind::LParen, "expected '('");
  if (!at(TokenKind::RParen)) {
    do {
      arguments.push_back(parseExpr());
    } while (consume(TokenKind::Comma));
  }
  expect(TokenKind::RParen, "expected ')' after arguments");
  return arguments;
}

ast::NodeId Parser::nextNodeId() {
  return nextNodeId_++;
}

} // namespace gura
