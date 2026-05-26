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

TEST_CASE("parser parses array literals and index reads") {
  requireParses(R"gura(
fn main(): i64 {
  let a: [i64] = [1, 2, 3]
  return a[1]
}
)gura");
}

TEST_CASE("parser parses mutable array index assignment") {
  requireParses(R"gura(
fn main(): i64 {
  let a: mut [i64] = [1, 2]
  a[1] = 9
  return a[1]
}
)gura");
}
