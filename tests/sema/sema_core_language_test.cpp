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

TEST_CASE("sema accepts iso move into freeze") {
  CHECK(checkSource(R"gura(
fn main(): imm Box {
  let x: iso Box = new iso Box
  return freeze(move x)
}
)gura"));
}

TEST_CASE("sema accepts iso move into merge inside active region") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  let parent: iso Box = new iso Box
  let child: iso Box = new iso Box
  enter parent as bridge {
    let y = merge(move child)
    return 0
  }
  return 0
}
)gura"));
}

TEST_CASE("sema accepts enter and explore on iso source") {
  CHECK(checkSource(R"gura(
fn main(): i64 {
  let r: iso Box = new iso Box
  enter r as b { return 1 }
  explore r as b { return 1 }
  return 0
}
)gura"));
}

TEST_CASE("sema rejects copying iso binding") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = x
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("cannot copy iso") != std::string::npos);
}

TEST_CASE("sema rejects use after move") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = move x
  let z = move x
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("already moved") != std::string::npos);
}

TEST_CASE("sema rejects freeze and merge without move") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = freeze(x)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("freeze requires move of an iso value") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = merge(x)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("merge requires move of an iso value") != std::string::npos);
}

TEST_CASE("sema rejects merge outside active region") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: iso Box = new iso Box
  let y = merge(move x)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("merge requires an active region") != std::string::npos);
}

TEST_CASE("sema rejects freeze and merge on non-iso") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: mut Box = new mut Box
  let y = freeze(move x)
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("freeze requires an iso") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let outer: iso Box = new iso Box
  let x: imm Box = new imm Box
  enter outer as bridge {
    let y = merge(move x)
    return 0
  }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("merge requires an iso") != std::string::npos);
}

TEST_CASE("sema rejects enter on non-iso and region-local escape") {
  std::string diagnostics;
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let x: mut Box = new mut Box
  enter x as b { return 1 }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("enter/explore source must be an iso binding") != std::string::npos);

  diagnostics.clear();
  CHECK_FALSE(checkSource(R"gura(
fn main(): i64 {
  let r: iso Box = new iso Box
  enter r as b { new mut Box }
  return 0
}
)gura", &diagnostics));
  CHECK(diagnostics.find("region block cannot return") != std::string::npos);
}
