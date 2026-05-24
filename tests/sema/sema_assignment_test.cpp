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

TEST_CASE("sema accepts assignment to var binding") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  var x: i64 = 1
  x = x + 1
  return x
}
)gura"));
}

TEST_CASE("sema rejects assignment to let binding") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: i64 = 1
  x = 2
  return x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot assign to let binding 'x'") != std::string::npos);
}

TEST_CASE("sema rejects assignment type mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  var x: i64 = 1
  x = true
  return x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("assignment type 'bool' does not match binding type 'i64'") != std::string::npos);
}

TEST_CASE("sema accepts assignment after move restoring var binding") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  var x: iso Box = new iso Box
  let y = move x
  x = new iso Box
  let z = move x
  return 0
}
)gura"));
}
