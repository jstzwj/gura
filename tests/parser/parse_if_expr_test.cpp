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

TEST_CASE("parser parses if expression with else") {
  requireParses(R"gura(
fn main(): i64 {
  if 1 < 2 { return 1 } else { return 2 }
}
)gura");
}

TEST_CASE("parser parses if expression without else") {
  requireParses(R"gura(
fn main(): unit {
  if true { none }
}
)gura");
}

TEST_CASE("parser parses nested else if") {
  requireParses(R"gura(
fn main(): i64 {
  if false { return 1 } else if true { return 2 } else { return 3 }
}
)gura");
}
