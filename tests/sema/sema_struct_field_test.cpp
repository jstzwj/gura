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

TEST_CASE("sema accepts struct field access and assignment") {
  CHECK(checkSource(R"gura(
struct Point {
  var x: i64
  var y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point
  p.x = 1
  p.y = 2
  return p.x + p.y
}
)gura"));
}

TEST_CASE("sema rejects missing fields") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point
  return p.y
}
)gura", &diagnostics));
  CHECK(diagnostics.find("type 'Point' has no field 'y'") != std::string::npos);
}

TEST_CASE("sema rejects field assignment type mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point
  p.x = true
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("assignment type 'bool' does not match field type 'i64'") != std::string::npos);
}

TEST_CASE("sema rejects reading iso fields as ordinary values") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Child {
  var x: i64
}

struct Parent {
  var child: iso Child
}

fn main(): i64 {
  let child: iso Child = new iso Child
  var p: mut Parent = new mut Parent { child: move child }
  let c = p.child
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot read iso field 'child'") != std::string::npos);
}

TEST_CASE("sema rejects assigning iso field without move") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Child {
  var x: i64
}

struct Parent {
  var child: iso Child
}

fn main(): i64 {
  let child: iso Child = new iso Child
  let next: iso Child = new iso Child
  var p: mut Parent = new mut Parent { child: move child }
  p.child = next
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("assignment to iso field 'child' requires move") != std::string::npos);
}

TEST_CASE("sema accepts assigning iso field with move") {
  CHECK(checkSource(R"gura(
struct Child {
  var x: i64
}

struct Parent {
  var child: iso Child
}

fn main(): i64 {
  let child: iso Child = new iso Child
  let next: iso Child = new iso Child
  var p: mut Parent = new mut Parent { child: move child }
  p.child = move next
  return 0
}
)gura"));
}

TEST_CASE("sema rejects iso field access without enter") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot access fields through iso without enter") != std::string::npos);
}
