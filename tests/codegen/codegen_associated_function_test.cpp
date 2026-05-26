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

TEST_CASE("codegen emits associated constructor functions") {
  const std::string ir = emitSource(R"gura(
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
)gura");

  CHECK(ir.find("define %Point @Point_new") != std::string::npos);
  CHECK(ir.find("call %Point @Point_new") != std::string::npos);
  CHECK(ir.find("define i64 @Point_sum") != std::string::npos);
  CHECK(ir.find("call i64 @Point_sum") != std::string::npos);
}

TEST_CASE("codegen emits associated functions calling associated functions") {
  const std::string ir = emitSource(R"gura(
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
)gura");

  CHECK(ir.find("define %Point @Point_new") != std::string::npos);
  CHECK(ir.find("define %Point @Point_origin") != std::string::npos);
  CHECK(ir.find("call %Point @Point_new") != std::string::npos);
  CHECK(ir.find("call %Point @Point_origin") != std::string::npos);
}
