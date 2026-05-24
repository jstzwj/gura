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

TEST_CASE("parser parses struct methods and method calls") {
  requireParses(R"gura(
struct Point {
  let x: i64
  let y: i64

  fn sum(self: imm): i64 {
    return self.x + self.y
  }
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1, y: 2 }
  return p.sum()
}
)gura");
}

TEST_CASE("parser parses method arguments") {
  requireParses(R"gura(
struct Counter {
  var value: i64

  fn add(self: mut, amount: i64): i64 {
    self.value = self.value + amount
    return self.value
  }
}

fn main(): i64 {
  var c: mut Counter = new mut Counter { value: 1 }
  return c.add(2)
}
)gura");
}
