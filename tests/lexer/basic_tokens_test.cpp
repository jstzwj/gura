#include "gura/lexer/Lexer.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura;

TEST_CASE("lexer recognizes keywords and capabilities") {
  Lexer lexer("fn main(): iso Tree { return none }");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 10);
  CHECK(tokens[0].kind == TokenKind::KwFn);
  CHECK(tokens[1].kind == TokenKind::Identifier);
  CHECK(tokens[5].kind == TokenKind::KwIso);
  CHECK(tokens[9].kind == TokenKind::KwNone);
}

TEST_CASE("lexer recognizes pau capability") {
  Lexer lexer("pau Node");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[0].kind == TokenKind::KwPau);
  CHECK(tokens[0].text == "pau");
}

TEST_CASE("lexer recognizes module and import keywords") {
  Lexer lexer("module app.main import std.core as core");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 8);
  CHECK(tokens[0].kind == TokenKind::KwModule);
  CHECK(tokens[2].kind == TokenKind::Dot);
  CHECK(tokens[4].kind == TokenKind::KwImport);
  CHECK(tokens[6].kind == TokenKind::Dot);
  CHECK(tokens[8].kind == TokenKind::KwAs);
}

TEST_CASE("lexer recognizes escaped identifiers") {
  Lexer lexer("let `type` = 1");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 3);
  CHECK(tokens[1].kind == TokenKind::Identifier);
  CHECK(tokens[1].text == "type");
}

TEST_CASE("lexer recognizes numeric literal suffixes") {
  Lexer lexer("0i32 42i64 1.2f32 3.14f64 5 6.7");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 7);
  CHECK(tokens[0].kind == TokenKind::IntegerLiteral);
  CHECK(tokens[0].text == "0i32");
  CHECK(tokens[1].kind == TokenKind::IntegerLiteral);
  CHECK(tokens[1].text == "42i64");
  CHECK(tokens[2].kind == TokenKind::FloatLiteral);
  CHECK(tokens[2].text == "1.2f32");
  CHECK(tokens[3].kind == TokenKind::FloatLiteral);
  CHECK(tokens[3].text == "3.14f64");
  CHECK(tokens[4].kind == TokenKind::IntegerLiteral);
  CHECK(tokens[5].kind == TokenKind::FloatLiteral);
}

TEST_CASE("lexer rejects malformed numeric literal suffixes") {
  Lexer lexer("1abc 1.2i32 1f32");
  const auto tokens = lexer.lexAll();
  REQUIRE(tokens.size() >= 4);
  CHECK(tokens[0].kind == TokenKind::Invalid);
  CHECK(tokens[1].kind == TokenKind::Invalid);
  CHECK(tokens[2].kind == TokenKind::Invalid);
}
