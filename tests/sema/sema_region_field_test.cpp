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

TEST_CASE("sema rejects moving opened iso source inside region") {
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
  CHECK(diagnostics.find("binding 'p' is currently open-borrowed") != std::string::npos);
}

TEST_CASE("sema rejects reading opened iso source inside region") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    let copy = p
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("binding 'p' is currently open-borrowed") != std::string::npos);
}

TEST_CASE("sema rejects reentering opened iso source inside region") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    enter p as again {
      return 0
    }
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("binding 'p' is currently open-borrowed") != std::string::npos);
}

TEST_CASE("sema rejects writing outer enter binding inside nested enter") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let outer: iso Point = new iso Point
  let inner: iso Point = new iso Point
  enter outer as o {
    enter inner as i {
      o.x = 1
      return 0
    }
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("field assignment requires a mut receiver") != std::string::npos);
}

TEST_CASE("sema rejects writing outer enter binding inside explore") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let outer: iso Point = new iso Point
  let inner: iso Point = new iso Point
  enter outer as o {
    explore inner as i {
      o.x = 1
      return 0
    }
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("field assignment requires a mut receiver") != std::string::npos);
}

TEST_CASE("sema rejects explore body new mut tail result") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  explore p as q {
    new mut Point
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("region block cannot return") != std::string::npos);
}

TEST_CASE("sema rejects storing paused field view in regular field") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

struct Holder {
  var point: mut Point
}

fn main(): i64 {
  let outer: iso Holder = new iso Holder
  let inner: iso Point = new iso Point
  enter outer as h {
    explore inner as p {
      h.point = p
      return 0
    }
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("field assignment requires a mut receiver") != std::string::npos);
}

TEST_CASE("sema rejects passing paused field view to mut parameter") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

struct Holder {
  var point: mut Point
}

fn take(p: mut Point): i64 {
  return p.x
}

fn main(): i64 {
  let h: iso Holder = new iso Holder
  explore h as view {
    return take(view.point)
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("argument 1 type 'Point' does not match expected type 'Point'") != std::string::npos);
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

TEST_CASE("sema rejects enter bridge reassignment to tmp value") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    q = new tmp Point
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("enter bridge reassignment requires a mut value") != std::string::npos);
}

TEST_CASE("sema rejects enter bridge reassignment to imm value") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    q = new imm Point
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("enter bridge reassignment requires a mut value") != std::string::npos);
}

TEST_CASE("sema rejects enter bridge reassignment to different type") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
struct Point {
  var x: i64
}

struct Other {
  var y: i64
}

fn main(): i64 {
  let p: iso Point = new iso Point
  enter p as q {
    let other: mut Other = new mut Other
    q = other
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("enter bridge reassignment must preserve bridge type") != std::string::npos);
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
