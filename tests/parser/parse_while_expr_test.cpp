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

TEST_CASE("parser parses while expression") {
  requireParses(R"gura(
fn main(): i64 {
  var i: i64 = 0
  while i < 3 {
    i = i + 1
  }
  return i
}
)gura");
}
