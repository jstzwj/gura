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

TEST_CASE("sema accepts if expression with matching branches") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  return if 1 < 2 { 1 } else { 2 }
}
)gura"));
}

TEST_CASE("sema accepts if expression without else as unit") {
  CHECK(checkSource(R"gura(
fn main(): unit {
  return if true { none }
}
)gura"));
}

TEST_CASE("sema rejects non-bool if condition") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  return if 1 { 1 } else { 2 }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("if condition must be bool") != std::string::npos);
}

TEST_CASE("sema rejects mismatched if branches") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  return if true { 1 } else { false }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("if branch type 'i64' does not match else branch type 'bool'") != std::string::npos);
}
