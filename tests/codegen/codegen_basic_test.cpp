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
