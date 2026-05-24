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

TEST_CASE("sema accepts while loops") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  var i: i64 = 0
  var acc: i64 = 0
  while i < 3 {
    acc = acc + i
    i = i + 1
  }
  return acc
}
)gura"));
}

TEST_CASE("sema rejects non-bool while condition") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  var i: i64 = 0
  while i {
    i = i + 1
  }
  return i
}
)gura", &diagnostics));
  CHECK(diagnostics.find("while condition must be bool") != std::string::npos);
}

TEST_CASE("sema treats while as unit") {
  CHECK(checkSource(R"gura(
fn main(): unit {
  return while false {
    none
  }
}
)gura"));
}
