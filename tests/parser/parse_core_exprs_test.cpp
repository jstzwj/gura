#include "gura/basic/Diagnostic.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura;

namespace {

void requireParses(std::string_view source) {
  Lexer lexer(source);
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  INFO(diagnostics.format());
  CHECK_FALSE(diagnostics.hasError());
  REQUIRE(file->declarations.size() == 1);
}

} // namespace

TEST_CASE("parser parses binding and move expressions") {
  requireParses(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = move x
  return 0
}
)gura");
}

TEST_CASE("parser parses freeze and merge expressions") {
  requireParses(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let frozen = freeze(move x)
  let y: iso Box = new iso Box
  let merged = merge(move y)
  return 0
}
)gura");
}

TEST_CASE("parser parses enter and explore expressions") {
  requireParses(R"gura(
fn main(): i64 {
  let r: iso Box = new iso Box
  enter r as b { return 1 }
  explore r as b { return 1 }
  return 0
}
)gura");
}
