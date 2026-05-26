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

std::string parseDiagnostics(std::string_view source) {
  Lexer lexer(source);
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  parser.parseSourceFile();
  return diagnostics.format();
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
  enter r as bridge { return 1 }
  explore r as view { return 1 }
  return 0
}
)gura");
}

TEST_CASE("parser treats bridge as an identifier") {
  requireParses(R"gura(
fn main(): i64 {
  let bridge: i64 = 1
  return bridge
}
)gura");
}

TEST_CASE("parser parses iso allocation strategies") {
  requireParses(R"gura(
fn main(): i64 {
  let r: iso Box = new iso<Arena> Box { value: 1 }
  return 0
}
)gura");
}

TEST_CASE("parser rejects region strategy without iso") {
  const std::string diagnostics = parseDiagnostics(R"gura(
fn main(): i64 {
  let r: mut Box = new mut<Arena> Box { value: 1 }
  return 0
}
)gura");

  CHECK(diagnostics.find("region allocation strategy requires 'iso'") != std::string::npos);
}

TEST_CASE("parser rejects region expressions without binding") {
  const std::string diagnostics = parseDiagnostics(R"gura(
fn main(): i64 {
  let r: iso Box = new iso Box
  enter r { return 1 }
  return 0
}
)gura");

  CHECK(diagnostics.find("expected 'as' before region binding name") != std::string::npos);
}
