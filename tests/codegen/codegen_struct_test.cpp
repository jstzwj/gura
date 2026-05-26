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

TEST_CASE("codegen emits pure-value struct layout and field loads") {
  const std::string ir = emitSource(R"gura(
struct Point {
  let x: i64
  let y: i64
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1, y: 2 }
  return p.x + p.y
}
)gura");

  CHECK(ir.find("%Point = type { i64, i64 }") != std::string::npos);
  CHECK(ir.find("alloca %Point") != std::string::npos);
  CHECK(ir.find("store %Point") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds %Point") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}
