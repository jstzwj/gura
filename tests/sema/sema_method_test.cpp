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

TEST_CASE("sema accepts imm method calls") {
  CHECK(checkSource(R"gura(
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
)gura"));
}

TEST_CASE("sema accepts mut method calls with arguments") {
  CHECK(checkSource(R"gura(
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
)gura"));
}

TEST_CASE("sema accepts pau method calls from explore") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64

  fn peek(self: pau): i64 {
    return self.x
  }
}

fn main(): i64 {
  let p: iso Point = new iso Point { x: 1 }
  explore p as q {
    return q.peek()
  }
  return 0
}
)gura"));
}

TEST_CASE("sema rejects unknown methods") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1 }
  return p.missing()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("type 'Point' has no method 'missing'") != std::string::npos);
}

TEST_CASE("sema rejects receiver capability mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64

  fn edit(self: mut): i64 {
    return self.x
  }
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1 }
  return p.edit()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'edit' requires receiver type 'Point'") != std::string::npos);
}

TEST_CASE("sema rejects method argument mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Counter {
  var value: i64

  fn add(self: mut, amount: i64): i64 {
    return self.value + amount
  }
}

fn main(): i64 {
  var c: mut Counter = new mut Counter { value: 1 }
  return c.add(true)
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'bool' does not match expected type 'i64'") != std::string::npos);
}
