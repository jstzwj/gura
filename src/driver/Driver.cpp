#include "gura/driver/Driver.h"

#include "gura/basic/Diagnostic.h"
#include "gura/basic/SourceManager.h"
#include "gura/codegen/LLVMCodeGen.h"
#include "gura/hir/Sema.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <fmt/format.h>

#include <iostream>
#include <string>

namespace gura {

int Driver::run(std::span<const char* const> args) {
  if (args.size() <= 1 || std::string_view(args[1]) == "--version") {
    return printVersion();
  }
  const std::string_view command(args[1]);
  if (args.size() < 3) {
    std::cerr << "missing input file\n";
    return 2;
  }
  if (command == "lex") {
    return lexFile(args[2]);
  }
  if (command == "parse") {
    return parseFile(args[2]);
  }
  if (command == "check") {
    return checkFile(args[2]);
  }
  if (command == "emit-llvm") {
    return emitLlvm(args[2]);
  }
  std::cerr << fmt::format("unknown command: {}\n", command);
  return 2;
}

int Driver::printVersion() {
  std::cout << "gura 0.1.0\n";
  return 0;
}

int Driver::lexFile(std::string_view path) {
  SourceManager sources;
  const auto id = sources.loadFile(std::string(path));
  Lexer lexer(sources.contents(id));
  for (const auto& token : lexer.lexAll()) {
    std::cout << fmt::format("{} '{}'\n", tokenKindName(token.kind), token.text);
  }
  return 0;
}

int Driver::parseFile(std::string_view path) {
  SourceManager sources;
  const auto id = sources.loadFile(std::string(path));
  DiagnosticEngine diagnostics;
  Lexer lexer(sources.contents(id));
  const auto tokens = lexer.lexAll();
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  if (diagnostics.hasError()) {
    std::cerr << diagnostics.format();
    return 1;
  }
  std::cout << fmt::format("parsed {} declaration(s)\n", file->declarations.size());
  return 0;
}

int Driver::checkFile(std::string_view path) {
  SourceManager sources;
  const auto id = sources.loadFile(std::string(path));
  DiagnosticEngine diagnostics;
  Lexer lexer(sources.contents(id));
  const auto tokens = lexer.lexAll();
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  hir::Sema sema(diagnostics);
  if (!sema.check(*file)) {
    std::cerr << diagnostics.format();
    return 1;
  }
  std::cout << "check ok\n";
  return 0;
}

int Driver::emitLlvm(std::string_view path) {
  SourceManager sources;
  const auto id = sources.loadFile(std::string(path));
  DiagnosticEngine diagnostics;
  Lexer lexer(sources.contents(id));
  const auto tokens = lexer.lexAll();
  Parser parser(tokens, diagnostics);
  const auto file = parser.parseSourceFile();
  hir::Sema sema(diagnostics);
  if (!sema.check(*file)) {
    std::cerr << diagnostics.format();
    return 1;
  }
  LLVMCodeGen codegen;
  std::cout << codegen.emitModule(*file);
  return 0;
}

} // namespace gura
