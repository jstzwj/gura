#include "gura/basic/Diagnostic.h"
#include "gura/codegen/LLVMCodeGen.h"
#include "gura/hir/Sema.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <catch2/catch_test_macros.hpp>

using namespace gura;

namespace {

std::string emitSource(std::string_view source) {
  Lexer lexer(source);
  const auto tokens = lexer.lexAll();
  DiagnosticEngine diagnostics;
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  hir::Sema sema(diagnostics);
  INFO(diagnostics.format());
  REQUIRE(file != nullptr);
  REQUIRE(sema.check(*file));
  LLVMCodeGen codegen;
  return codegen.emitModule(*file);
}

} // namespace

TEST_CASE("codegen emits i64 functions and arithmetic") {
  const std::string ir = emitSource(R"gura(
fn add(a: i64, b: i64): i64 {
  return a + b
}
)gura");

  CHECK(ir.find("define i64 @add") != std::string::npos);
  CHECK(ir.find("add nsw i64") != std::string::npos);
  CHECK(ir.find("ret i64") != std::string::npos);
}

TEST_CASE("codegen emits function calls") {
  const std::string ir = emitSource(R"gura(
fn add(a: i64, b: i64): i64 {
  return a + b
}

fn main(): i64 {
  return add(1, 2)
}
)gura");

  CHECK(ir.find("call i64 @add") != std::string::npos);
}

TEST_CASE("codegen emits puts calls with string literals") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  puts("hello")
  return 0
}
)gura");

  CHECK(ir.find("@puts") != std::string::npos);
  CHECK(ir.find("call i32 @puts") != std::string::npos);
  CHECK(ir.find("hello") != std::string::npos);
}

TEST_CASE("codegen emits core string output builtins") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  print("hello")
  println("world")
  return 0
}
)gura");

  CHECK(ir.find("@printf") != std::string::npos);
  CHECK(ir.find("hello") != std::string::npos);
  CHECK(ir.find("world") != std::string::npos);
}

TEST_CASE("codegen emits qualified std core builtins") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  std.core.print("hello")
  std.core.println("world")
  std.core.print_i64(1)
  std.core.println_i64(2)
  let value: i64 = std.core.readln_i64()
  return value
}
)gura");

  CHECK(ir.find("@printf") != std::string::npos);
  CHECK(ir.find("@scanf") != std::string::npos);
  CHECK(ir.find("hello") != std::string::npos);
  CHECK(ir.find("world") != std::string::npos);
  CHECK(ir.find("alloca i64") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}

TEST_CASE("codegen emits core i64 I/O builtins") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  print_i64(1)
  println_i64(2)
  let value: i64 = readln_i64()
  return value
}
)gura");

  CHECK(ir.find("@printf") != std::string::npos);
  CHECK(ir.find("@scanf") != std::string::npos);
  CHECK(ir.find("alloca i64") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}

TEST_CASE("codegen emits comparisons and branches") {
  const std::string ir = emitSource(R"gura(
fn abs(x: i64): i64 {
  if x < 0 {
    return 0 - x
  } else {
    return x
  }
}
)gura");

  CHECK(ir.find("define i64 @abs") != std::string::npos);
  CHECK(ir.find("icmp slt i64") != std::string::npos);
  CHECK(ir.find("br i1") != std::string::npos);
  CHECK(ir.find("sub nsw i64") != std::string::npos);
}

TEST_CASE("codegen emits i32 arithmetic") {
  const std::string ir = emitSource(R"gura(
fn add32(a: i32, b: i32): i32 {
  return a + b
}
)gura");

  CHECK(ir.find("define i32 @add32") != std::string::npos);
  CHECK(ir.find("add nsw i32") != std::string::npos);
}

TEST_CASE("codegen emits floating point arithmetic and comparisons") {
  const std::string ir = emitSource(R"gura(
fn addf(a: f64, b: f64): f64 {
  return a + b
}

fn cmpf(a: f32, b: f32): bool {
  return a < b
}
)gura");

  CHECK(ir.find("define double @addf") != std::string::npos);
  CHECK(ir.find("fadd double") != std::string::npos);
  CHECK(ir.find("define i1 @cmpf") != std::string::npos);
  CHECK(ir.find("fcmp olt float") != std::string::npos);
}

TEST_CASE("codegen emits region runtime calls") {
  const std::string ir = emitSource(R"gura(
struct Box {
  var value: i64
}

fn main(): i64 {
  let r: iso Box = new iso<Arena> Box { value: 1 }
  enter r as bridge {
    bridge.value = 2
    return 0
  }
  return 0
}
)gura");

  CHECK(ir.find("@__gura_region_new_iso") != std::string::npos);
  CHECK(ir.find("@__gura_region_set_bridge_type") != std::string::npos);
  CHECK(ir.find("@__gura_region_enter") != std::string::npos);
  CHECK(ir.find("@__gura_region_bridge") != std::string::npos);
  CHECK(ir.find("@__gura_region_exit") != std::string::npos);
}

TEST_CASE("codegen sets bridge type metadata") {
  const std::string ir = emitSource(R"gura(
struct Box {
  var value: i64
}

struct Bag {
  var value: i64
}

fn main(): i64 {
  let box: iso Box = new iso Box { value: 1 }
  let bag: iso Bag = new iso Bag { value: 2 }
  return 0
}
)gura");

  CHECK(ir.find("call void @__gura_region_set_bridge_type") != std::string::npos);
  CHECK(ir.find(", i64 1)") != std::string::npos);
  CHECK(ir.find(", i64 2)") != std::string::npos);
}

TEST_CASE("codegen passes region allocation strategies") {
  const std::string ir = emitSource(R"gura(
struct Box {
  var value: i64
}

fn main(): i64 {
  let defaulted: iso Box = new iso Box { value: 1 }
  let arena: iso Box = new iso<Arena> Box { value: 2 }
  let rc: iso Box = new iso<RC> Box { value: 3 }
  let gc: iso Box = new iso<GC> Box { value: 4 }
  let manual: iso Box = new iso<Manual> Box { value: 5 }
  return 0
}
)gura");

  CHECK(ir.find("call ptr @__gura_region_new_iso(i64") != std::string::npos);
  CHECK(ir.find(", i32 0)") != std::string::npos);
  CHECK(ir.find(", i32 1)") != std::string::npos);
  CHECK(ir.find(", i32 2)") != std::string::npos);
  CHECK(ir.find(", i32 3)") != std::string::npos);
}

TEST_CASE("codegen emits freeze and merge runtime calls") {
  const std::string ir = emitSource(R"gura(
struct Box {
  var value: i64
}

fn main(): i64 {
  let parent: iso Box = new iso Box { value: 1 }
  let child: iso Box = new iso Box { value: 2 }
  let frozen: imm Box = freeze(move parent)
  let outer: iso Box = new iso Box { value: 3 }
  enter outer as bridge {
    let merged = merge(move child)
    bridge.value = 4
    return 0
  }
  return 0
}
)gura");

  CHECK(ir.find("@__gura_region_freeze") != std::string::npos);
  CHECK(ir.find("@__gura_region_merge") != std::string::npos);
}
