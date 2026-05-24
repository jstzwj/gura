#include "gura/basic/Diagnostic.h"
#include "gura/hir/Sema.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura;

namespace {

bool checkSource(std::string_view source, std::string* diagnosticsText = nullptr) {
  Lexer lexer(source);
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  hir::Sema sema(diagnostics);
  const bool ok = file != nullptr && sema.check(*file);
  if (diagnosticsText != nullptr) {
    *diagnosticsText = diagnostics.format();
  }
  return ok;
}

} // namespace

TEST_CASE("sema accepts impl method calls") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

impl Point {
  fn sum(self: imm): i64 {
    return self.x + self.y
  }
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1, y: 2 }
  return p.sum()
}
)gura"));
}

TEST_CASE("sema accepts mut impl methods") {
  CHECK(checkSource(R"gura(
struct Counter {
  var value: i64
}

impl Counter {
  fn add(self: mut, amount: i64): i64 {
    self.value = self.value + amount
    return self.value
  }
}

fn main(): i64 {
  var c: mut Counter = new mut Counter { value: 1 }
  return c.add(2)
}
)gura"));
}

TEST_CASE("sema rejects impl for unknown type") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
impl Missing {
  fn get(self: imm): i64 {
    return 0
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("impl target type 'Missing' is not defined") != std::string::npos);
}

TEST_CASE("sema rejects duplicate methods across struct and impl") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64

  fn get(self: imm): i64 {
    return self.x
  }
}

impl Point {
  fn get(self: imm): i64 {
    return self.x
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'get' is already defined") != std::string::npos);
}

TEST_CASE("sema accepts impl associated functions without self") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
}

impl Point {
  fn get(): i64 {
    return 0
  }
}

fn main(): i64 {
  return Point.get()
}
)gura"));
}
