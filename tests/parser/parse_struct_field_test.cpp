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

TEST_CASE("parser parses struct declarations and field access") {
  requireParses(R"gura(
struct Point {
  var x: i64
  var y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point
  p.x = 1
  p.y = 2
  return p.x + p.y
}
)gura");
}
