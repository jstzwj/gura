#include "gura/basic/Diagnostic.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura;

TEST_CASE("parser parses minimal main function") {
  Lexer lexer("fn main(): i64 { return 0 }");
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  CHECK_FALSE(diagnostics.hasError());
  INFO(diagnostics.format());
  REQUIRE(file->declarations.size() == 1);
}
