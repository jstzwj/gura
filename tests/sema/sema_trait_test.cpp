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

TEST_CASE("sema accepts complete trait impl") {
  CHECK(checkSource(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
  fn name(self: imm): i64 {
    return self.x
  }
}

fn main(): i64 {
  var p: imm Point = new imm Point { x: 1 }
  return p.name()
}
)gura"));
}

TEST_CASE("sema rejects unknown trait in trait impl") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

impl Missing for Point {
  fn name(self: imm): i64 {
    return self.x
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("trait 'Missing' is not defined") != std::string::npos);
}

TEST_CASE("sema rejects unknown trait impl target type") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

impl Named for Missing {
  fn name(self: imm): i64 {
    return 0
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("impl target type 'Missing' is not defined") != std::string::npos);
}

TEST_CASE("sema rejects missing trait methods") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
}
)gura", &diagnostics));
  CHECK(diagnostics.find("impl of trait 'Named' for type 'Point' is missing method 'name'") != std::string::npos);
}

TEST_CASE("sema rejects trait method return mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
  fn name(self: imm): bool {
    return true
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'name' signature does not match trait 'Named'") != std::string::npos);
}

TEST_CASE("sema rejects trait method receiver mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
  fn name(self: mut): i64 {
    return self.x
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'name' signature does not match trait 'Named'") != std::string::npos);
}

TEST_CASE("sema rejects trait method parameter mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm, suffix: i64): i64
}

impl Named for Point {
  fn name(self: imm, suffix: bool): i64 {
    return self.x
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'name' signature does not match trait 'Named'") != std::string::npos);
}

TEST_CASE("sema accepts generic trait-bound method calls") {
  CHECK(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

fn get_name<T: Named>(value: imm T): i64 {
  return value.name()
}
)gura"));
}

TEST_CASE("sema accepts multiple generic trait bounds") {
  CHECK(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

trait Tagged {
  fn tag(self: imm): i64
}

fn describe<T: Named + Tagged>(value: imm T): i64 {
  return value.name() + value.tag()
}
)gura"));
}

TEST_CASE("sema accepts multiple generic parameters") {
  CHECK(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

trait Tagged {
  fn tag(self: imm): i64
}

fn combine<T: Named, U: Tagged>(x: imm T, y: imm U): i64 {
  return x.name() + y.tag()
}
)gura"));
}

TEST_CASE("sema rejects unknown generic trait bounds") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn bad<T: Missing>(value: imm T): i64 {
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("trait 'Missing' is not defined") != std::string::npos);
}

TEST_CASE("sema rejects methods outside generic trait bounds") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

fn bad<T: Named>(value: imm T): i64 {
  return value.missing()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("generic type 'T' has no method 'missing'") != std::string::npos);
}

TEST_CASE("sema rejects generic receiver capability mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Editable {
  fn edit(self: mut): i64
}

fn bad<T: Editable>(value: imm T): i64 {
  return value.edit()
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'edit' requires") != std::string::npos);
}

TEST_CASE("sema rejects generic method argument mismatch") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Accumulator {
  fn add(self: mut, value: i64): i64
}

fn bad<T: Accumulator>(value: mut T): i64 {
  return value.add(true)
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'bool' does not match expected type 'i64'") != std::string::npos);
}

TEST_CASE("sema rejects generic field access") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

fn bad<T: Named>(value: imm T): i64 {
  return value.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot access field 'x' on generic type 'T'") != std::string::npos);
}

TEST_CASE("sema rejects generic function calls") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Person {
  let id: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Person {
  fn name(self: imm): i64 {
    return self.id
  }
}

fn get_name<T: Named>(value: imm T): i64 {
  return value.name()
}

fn main(): i64 {
  var p: imm Person = new imm Person { id: 1 }
  return get_name(p)
}
)gura", &diagnostics));
  CHECK(diagnostics.find("calling generic function 'get_name' is not supported yet") != std::string::npos);
}

TEST_CASE("sema rejects generic methods") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
trait Named {
  fn name(self: imm): i64
}

struct Box {
  let id: i64

  fn get<T: Named>(self: imm, value: imm T): i64 {
    return value.name()
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("generic methods are not supported yet") != std::string::npos);
}

TEST_CASE("sema rejects extra methods in trait impl") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

trait Named {
  fn name(self: imm): i64
}

impl Named for Point {
  fn name(self: imm): i64 {
    return self.x
  }

  fn extra(self: imm): i64 {
    return self.x
  }
}
)gura", &diagnostics));
  CHECK(diagnostics.find("method 'extra' is not a member of trait 'Named'") != std::string::npos);
}
