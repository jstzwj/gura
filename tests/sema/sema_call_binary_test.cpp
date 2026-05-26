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

TEST_CASE("sema accepts checked function calls") {
  CHECK(checkSource(R"gura(
fn id(x: i64): i64 { return x }
fn main(): i64 { return id(42) }
)gura"));
}

TEST_CASE("sema rejects bad function calls") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn id(x: i64): i64 { return x }
fn main(): i64 { return id(true) }
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'bool'") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn id(x: i64): i64 { return x }
fn main(): i64 { return id(1, 2) }
)gura", &diagnostics));
  CHECK(diagnostics.find("expects 1 argument") != std::string::npos);
}

TEST_CASE("sema checks return types") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 { return true }
)gura", &diagnostics));
  CHECK(diagnostics.find("return type 'bool'") != std::string::npos);
}

TEST_CASE("sema accepts puts with string literals") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  puts("hello")
  return 0
}
)gura"));
}

TEST_CASE("sema rejects puts with non-string arguments") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  puts(1)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'i64' does not match expected type 'cstring'") != std::string::npos);
}

TEST_CASE("sema accepts core string output builtins") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  print("hello")
  println("world")
  return 0
}
)gura"));
}

TEST_CASE("sema accepts core i64 I/O builtins") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  print_i64(1)
  println_i64(2)
  let value: i64 = readln_i64()
  return value
}
)gura"));
}

TEST_CASE("sema accepts qualified std core builtins") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  std.core.print("hello")
  std.core.println("world")
  std.core.print_i64(1)
  std.core.println_i64(2)
  let value: i64 = std.core.readln_i64()
  return value
}
)gura"));
}

TEST_CASE("sema rejects qualified std core builtin argument mismatches") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  std.core.println(1)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'i64' does not match expected type 'cstring'") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  std.core.println_i64("hello")
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'cstring' does not match expected type 'i64'") != std::string::npos);
}

TEST_CASE("sema rejects core output builtin argument mismatches") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  println(1)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'i64' does not match expected type 'cstring'") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  println_i64("hello")
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'cstring' does not match expected type 'i64'") != std::string::npos);
}

TEST_CASE("sema accepts i64 arithmetic and comparisons") {
  CHECK(checkSource(R"gura(
fn main(): bool { return 1 + 2 * 3 == 7 }
)gura"));
}

TEST_CASE("sema rejects invalid binary operands") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 { return true + 1 }
)gura", &diagnostics));
  CHECK(diagnostics.find("operator '+' requires matching numeric operands") != std::string::npos);
}

TEST_CASE("sema accepts numeric literal suffixes") {
  CHECK(checkSource("fn main(): i32 { return 0i32 }"));
  CHECK(checkSource("fn main(): i64 { return 0i64 }"));
  CHECK(checkSource("fn main(): f32 { return 1.2f32 }"));
  CHECK(checkSource("fn main(): f64 { return 1.2f64 }"));
}

TEST_CASE("sema uses default numeric literal types") {
  CHECK(checkSource("fn main(): i64 { return 0 }"));
  CHECK(checkSource("fn main(): f64 { return 1.2 }"));

  std::string diagnostics;
  CHECK_FALSE(checkSource("fn main(): i32 { return 0 }", &diagnostics));
  CHECK(diagnostics.find("return type 'i64' does not match function return type 'i32'") != std::string::npos);
}

TEST_CASE("sema checks numeric operand types") {
  CHECK(checkSource("fn main(): i32 { return 1i32 + 2i32 }"));
  CHECK(checkSource("fn main(): f64 { return 1.0f64 + 2.0f64 }"));
  CHECK(checkSource("fn main(): bool { return 1.0f32 < 2.0f32 }"));

  std::string diagnostics;
  CHECK_FALSE(checkSource("fn main(): i64 { return 1i32 + 2i64 }", &diagnostics));
  CHECK(diagnostics.find("operator '+' requires matching numeric operands") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource("fn main(): f32 { return 1.0f32 % 2.0f32 }", &diagnostics));
  CHECK(diagnostics.find("operator '%' requires matching integer operands") != std::string::npos);
}
