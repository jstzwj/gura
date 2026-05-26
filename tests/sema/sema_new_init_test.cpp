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

TEST_CASE("sema accepts complete new field initializers") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1, y: 2 }
  return p.x + p.y
}
)gura"));
}

TEST_CASE("sema rejects unknown initialized fields") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1, y: 2 }
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("type 'Point' has no field 'y'") != std::string::npos);
}

TEST_CASE("sema rejects missing initialized fields") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1 }
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("missing initializer for field 'y'") != std::string::npos);
}

TEST_CASE("sema rejects duplicate initialized fields") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1, x: 2 }
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("field 'x' is initialized more than once") != std::string::npos);
}

TEST_CASE("sema rejects initializer type mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: true }
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("initializer type 'bool' does not match field type 'i64'") != std::string::npos);
}

TEST_CASE("sema accepts iso allocation strategies") {
  CHECK(checkSource(R"gura(
struct Box {
  let value: i64
}

fn main(): i64 {
  let arena: iso Box = new iso<Arena> Box { value: 1 }
  let rc: iso Box = new iso<RC> Box { value: 2 }
  let gc: iso Box = new iso<GC> Box { value: 3 }
  let manual: iso Box = new iso<Manual> Box { value: 4 }
  return 0
}
)gura"));
}

TEST_CASE("sema rejects unknown iso allocation strategy") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Box {
  let value: i64
}

fn main(): i64 {
  let b: iso Box = new iso<Foo> Box { value: 1 }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("unknown region allocation strategy 'Foo'") != std::string::npos);
}
