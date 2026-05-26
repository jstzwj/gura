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
  REQUIRE(file != nullptr);
  REQUIRE(file->declarations.size() >= 1);
}

} // namespace

TEST_CASE("parser parses trait declarations") {
  requireParses(R"gura(
trait Named {
  fn name(self: imm): i64
}
)gura");
}

TEST_CASE("parser parses trait impl declarations") {
  requireParses(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
  fn name(self: imm): i64 {
    return self.x
  }
}
)gura");
}

TEST_CASE("parser parses generic trait bounds") {
  requireParses(R"gura(
trait Named {
  fn name(self: imm): i64
}

fn get_name[T: Named](value: imm T): i64 {
  return value.name()
}
)gura");
}

TEST_CASE("parser parses multiple generic trait bounds") {
  requireParses(R"gura(
trait Named {
  fn name(self: imm): i64
}

trait Tagged {
  fn tag(self: imm): i64
}

fn describe[T: Named + Tagged](value: imm T): i64 {
  return value.name() + value.tag()
}
)gura");
}

TEST_CASE("parser parses multiple generic parameters") {
  requireParses(R"gura(
trait Named {
  fn name(self: imm): i64
}

trait Tagged {
  fn tag(self: imm): i64
}

fn combine[T: Named, U: Tagged](x: imm T, y: imm U): i64 {
  return x.name() + y.tag()
}
)gura");
}

TEST_CASE("parser still parses inherent impl declarations") {
  requireParses(R"gura(
struct Point {
  let x: i64
}

impl Point {
  fn get(self: imm): i64 {
    return self.x
  }
}
)gura");
}
