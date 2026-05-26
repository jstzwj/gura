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

TEST_CASE("codegen emits array literals and index reads") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  let a: [i64] = [1, 2, 3]
  return a[1]
}
)gura");

  CHECK(ir.find("[3 x i64]") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds [3 x i64]") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}

TEST_CASE("codegen emits array index assignment") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  let a: mut [i64] = [1, 2]
  a[0] = 9
  return a[0]
}
)gura");

  CHECK(ir.find("[2 x i64]") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds [2 x i64]") != std::string::npos);
  CHECK(ir.find("store i64 9") != std::string::npos);
}

TEST_CASE("codegen emits mutable array function parameters") {
  const std::string ir = emitSource(R"gura(
fn swap(a: mut [i64], i: i64, j: i64): unit {
  let saved: i64 = a[i]
  a[i] = a[j]
  a[j] = saved
}

fn main(): i64 {
  let a: mut [i64] = [10, 20]
  swap(a, 0, 1)
  return a[0]
}
)gura");

  CHECK(ir.find("define void @swap(ptr") != std::string::npos);
  CHECK(ir.find("call void @swap(ptr") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds i64") != std::string::npos);
}

TEST_CASE("codegen emits local array swap") {
  const std::string ir = emitSource(R"gura(
fn main(): i64 {
  let a: mut [i64] = [10, 20]
  let saved: i64 = a[0]
  a[0] = a[1]
  a[1] = saved
  return a[0]
}
)gura");

  CHECK(ir.find("[2 x i64]") != std::string::npos);
  CHECK(ir.find("store i64 %array.load") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}
