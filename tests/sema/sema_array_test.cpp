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

TEST_CASE("sema accepts array literals and index reads") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  let a: [i64] = [1, 2, 3]
  return a[1]
}
)gura"));
}

TEST_CASE("sema accepts mutable array index assignment") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  let a: mut [i64] = [1, 2, 3]
  a[0] = a[1]
  return a[0]
}
)gura"));
}

TEST_CASE("sema accepts local array swap") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  let a: mut [i64] = [10, 20]
  let saved: i64 = a[0]
  a[0] = a[1]
  a[1] = saved
  return a[0]
}
)gura"));
}

TEST_CASE("sema accepts mutable array function parameters") {
  CHECK(checkSource(R"gura(
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
)gura"));
}

TEST_CASE("sema rejects immutable array function parameters") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn first(a: [i64]): i64 {
  return a[0]
}
)gura", &diagnostics));
  CHECK(diagnostics.find("array parameters currently require mut [i64]") != std::string::npos);
}

TEST_CASE("sema rejects empty array literals") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let a: [i64] = []
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot infer empty array literal type") != std::string::npos);
}

TEST_CASE("sema rejects mixed array element types") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let a: [i64] = [1, true]
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("array elements must have matching type") != std::string::npos);
}

TEST_CASE("sema rejects index assignment to immutable arrays") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let a: [i64] = [1, 2]
  a[0] = 3
  return a[0]
}
)gura", &diagnostics));
  CHECK(diagnostics.find("array index assignment requires a mut array") != std::string::npos);
}

TEST_CASE("sema rejects non-integer array indices") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let a: mut [i64] = [1, 2]
  a[true] = 3
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("array index must be an integer") != std::string::npos);
}
