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

TEST_CASE("parser parses new field initializers") {
  requireParses(R"gura(
struct Point {
  let x: i64
  let y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1, y: 2 }
  return p.x + p.y
}
)gura");
}
