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

TEST_CASE("codegen emits struct field assignment stores") {
  const std::string ir = emitSource(R"gura(
struct Point {
  var x: i64
  var y: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point { x: 1, y: 2 }
  p.x = 10
  p.y = p.x + 1
  return p.y
}
)gura");

  CHECK(ir.find("%Point = type { i64, i64 }") != std::string::npos);
  CHECK(ir.find("getelementptr inbounds %Point") != std::string::npos);
  CHECK(ir.find("store i64 10") != std::string::npos);
  CHECK(ir.find("store i64 %addtmp") != std::string::npos);
  CHECK(ir.find("load i64") != std::string::npos);
}
