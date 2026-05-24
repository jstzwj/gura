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

TEST_CASE("codegen emits local variable loads and stores") {
  const std::string ir = emitSource(R"gura(
fn bump(x: i64): i64 {
  var y: i64 = x
  y = y + 1
  return y
}
)gura");

  CHECK(ir.find("alloca i64") != std::string::npos);
  CHECK(ir.find("store i64") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
  CHECK(ir.find("add nsw i64") != std::string::npos);
}

TEST_CASE("codegen emits while loops") {
  const std::string ir = emitSource(R"gura(
fn sum_to(n: i64): i64 {
  var i: i64 = 0
  var acc: i64 = 0
  while i < n {
    acc = acc + i
    i = i + 1
  }
  return acc
}
)gura");

  CHECK(ir.find("while.cond") != std::string::npos);
  CHECK(ir.find("while.body") != std::string::npos);
  CHECK(ir.find("while.end") != std::string::npos);
  CHECK(ir.find("br label") != std::string::npos);
  CHECK(ir.find("icmp slt i64") != std::string::npos);
}
