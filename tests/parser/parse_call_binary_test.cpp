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
  REQUIRE(file->declarations.size() >= 1);
}

} // namespace

TEST_CASE("parser parses function calls") {
  requireParses(R"gura(
fn add(a: i64, b: i64): i64 { return a }
fn main(): i64 {
  return add(1, 2)
}
)gura");
}

TEST_CASE("parser parses qualified std core builtin calls") {
  requireParses(R"gura(
fn main(): i64 {
  std.core.println_i64(42)
  return 0
}
)gura");
}

TEST_CASE("parser parses binary expressions") {
  requireParses(R"gura(
fn main(): i64 {
  return 1 + 2 * 3
}
)gura");
}
