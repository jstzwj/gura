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

TEST_CASE("parser parses module and import headers") {
  Lexer lexer(R"gura(
module app.main
import std.core
import math.vector as vec

fn main(): i64 { return 0 }
)gura");
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  CHECK_FALSE(diagnostics.hasError());
  INFO(diagnostics.format());
  REQUIRE(file->explicitModule.has_value());
  CHECK(file->explicitModule->segments == std::vector<std::string>{"app", "main"});
  REQUIRE(file->imports.size() == 2);
  CHECK(file->imports[0].path.segments == std::vector<std::string>{"std", "core"});
  CHECK(file->imports[0].alias == std::nullopt);
  CHECK(file->imports[1].path.segments == std::vector<std::string>{"math", "vector"});
  REQUIRE(file->imports[1].alias.has_value());
  CHECK(*file->imports[1].alias == "vec");
  REQUIRE(file->declarations.size() == 1);
}

TEST_CASE("parser rejects module and import after declarations") {
  Lexer lexer(R"gura(
fn main(): i64 { return 0 }
import std.core
module late
)gura");
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  parser.parseSourceFile();
  CHECK(diagnostics.hasError());
  const std::string text = diagnostics.format();
  CHECK(text.find("import declaration must appear before declarations") != std::string::npos);
  CHECK(text.find("module declaration must appear before imports and declarations") != std::string::npos);
}
