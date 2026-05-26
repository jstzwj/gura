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

TEST_CASE("codegen emits receiver method calls") {
  const std::string ir = emitSource(R"gura(
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
)gura");

  CHECK(ir.find("define i64 @Point_sum") != std::string::npos);
  CHECK(ir.find("call i64 @Point_sum") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds %Point") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}

TEST_CASE("codegen skips generic functions") {
  const std::string ir = emitSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

fn get_name[T: Named](value: imm T): i64 {
  return value.name()
}

fn main(): i64 {
  return 0
}
)gura");

  CHECK(ir.find("@get_name") == std::string::npos);
  CHECK(ir.find("define i64 @main") != std::string::npos);
}

TEST_CASE("codegen emits mut receiver method calls with arguments") {
  const std::string ir = emitSource(R"gura(
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
)gura");

  CHECK(ir.find("define i64 @Counter_add") != std::string::npos);
  CHECK(ir.find("call i64 @Counter_add") != std::string::npos);
  CHECK(ir.find("store i64 %addtmp") != std::string::npos);
}
