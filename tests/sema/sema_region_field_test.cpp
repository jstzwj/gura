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

TEST_CASE("sema exposes enter binding as mut") {
  CHECK(checkSource(R"gura(
struct Point {
  var x: i64
  var y: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    q.x = 1
    q.y = 2
    return q.x + q.y
  }
  return 0
}
)gura"));
}

TEST_CASE("sema exposes explore binding as pau readable but not writable") {
  CHECK(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  explore p as q {
    return q.x
  }
  return 0
}
)gura"));

  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  explore p as q {
    q.x = 1
    return q.x
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("field assignment requires a mut receiver") != std::string::npos);
}

TEST_CASE("sema rejects region-local bridge escape") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): mut Point {
  let p: iso Point = new iso Point
  enter p as q {
    return q
  }
  return new mut Point
}
)gura", &diagnostics));
  CHECK(diagnostics.find("region-local value cannot escape its region") != std::string::npos);
}

TEST_CASE("sema rejects using opened iso source inside region") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    let moved = move p
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("already moved") != std::string::npos);
}

TEST_CASE("sema rejects explore bridge reassignment") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  explore p as q {
    q = new mut Point
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot assign to let binding") != std::string::npos);
}

TEST_CASE("sema accepts enter bridge reassignment to same mut type") {
  CHECK(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    let next: mut Point = new mut Point
    q = next
    return 0
  }
  return 0
}
)gura"));
}

TEST_CASE("sema rejects assignment to let fields") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  let x: i64
}

fn main(): i64 {
  var p: mut Point = new mut Point
  p.x = 1
  return p.x
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot assign to let field 'x'") != std::string::npos);
}
