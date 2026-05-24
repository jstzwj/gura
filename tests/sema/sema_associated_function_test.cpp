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

TEST_CASE("sema accepts associated constructor functions") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

impl Point {
  fn new(x: i64, y: i64): imm Point {
    return new imm Point { x: x, y: y }
  }

  fn sum(self: imm): i64 {
    return self.x + self.y
  }
}

fn main(): i64 {
  var p: imm Point = Point.new(1, 2)
  return p.sum()
}
)gura"));
}

TEST_CASE("sema accepts associated functions calling associated functions") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

impl Point {
  fn new(x: i64, y: i64): imm Point {
    return new imm Point { x: x, y: y }
  }

  fn origin(): imm Point {
    return Point.new(0, 0)
  }
}

fn main(): i64 {
  var p: imm Point = Point.origin()
  return p.x
}
)gura"));
}

TEST_CASE("sema rejects unknown associated functions") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): imm Point {
  return Point.missing()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("type 'Point' has no method 'missing'") != std::string::npos);
}

TEST_CASE("sema rejects associated function argument mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

impl Point {
  fn new(x: i64): imm Point {
    return new imm Point { x: x }
  }
}

fn main(): imm Point {
  return Point.new(true)
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'bool' does not match expected type 'i64'") != std::string::npos);
}

TEST_CASE("sema rejects receiver methods called on a type") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

impl Point {
  fn get(self: imm): i64 {
    return self.x
  }
}

fn main(): i64 {
  return Point.get()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'get' requires a receiver") != std::string::npos);
}

TEST_CASE("sema rejects associated functions called on a value") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

impl Point {
  fn new(x: i64): imm Point {
    return new imm Point { x: x }
  }
}

fn main(): imm Point {
  var p: imm Point = new imm Point { x: 1 }
  return p.new(2)
}
)gura", &diagnostics));
  CHECK(diagnostics.find("associated function 'new' must be called on type 'Point'") != std::string::npos);
}
