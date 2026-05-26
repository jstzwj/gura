#include "gura/driver/Driver.h"

#include "gura/basic/Diagnostic.h"
#include "gura/basic/SourceManager.h"
#include "gura/codegen/LLVMCodeGen.h"
#include "gura/hir/Sema.h"
#include "gura/lexer/Lexer.h"
#include "gura/parser/Parser.h"

#include <fmt/format.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <unistd.h>

namespace gura {

namespace {

bool hasGenericFunctions(const ast::SourceFile& file) {
  for (const auto& decl : file.declarations) {
    const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get());
    if (fn != nullptr && !fn->genericParams.empty()) {
      return true;
    }
  }
  return false;
}

const ast::FnDecl* findMain(const ast::SourceFile& file) {
  for (const auto& decl : file.declarations) {
    const auto* fn = dynamic_cast<const ast::FnDecl*>(decl.get());
    if (fn != nullptr && fn->name == "main") {
      return fn;
    }
  }
  return nullptr;
}

bool isExecutableMain(const ast::FnDecl& fn) {
  return fn.genericParams.empty() && fn.params.empty() && fn.returnType != nullptr && fn.returnType->capability == ast::Capability::None && fn.returnType->name == "i64";
}

std::string shellQuote(std::string_view value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

std::filesystem::path temporaryIrPath() {
  static int counter = 0;
  return std::filesystem::temp_directory_path() / fmt::format("gura-{}-{}.ll", static_cast<long>(::getpid()), counter++);
}

class TemporaryFile {
public:
  explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}
  ~TemporaryFile() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  std::filesystem::path path_;
};

} // namespace

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
  if (command == "build") {
    return buildExecutable(args.subspan(2));
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
  if (hasGenericFunctions(*file)) {
    std::cerr << "LLVM codegen for generic functions is not supported yet\n";
    return 1;
  }
  LLVMCodeGen codegen;
  std::cout << codegen.emitModule(*file);
  return 0;
}

int Driver::buildExecutable(std::span<const char* const> args) {
  if (args.empty()) {
    std::cerr << "missing input file\n";
    return 2;
  }
  std::filesystem::path inputPath(args[0]);
  std::filesystem::path outputPath = inputPath.stem();
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg(args[i]);
    if (arg != "-o") {
      std::cerr << fmt::format("unknown build option: {}\n", arg);
      return 2;
    }
    if (i + 1 >= args.size()) {
      std::cerr << "missing output path after -o\n";
      return 2;
    }
    outputPath = args[++i];
  }
  if (!LLVMCodeGen::isAvailable()) {
    std::cerr << "LLVM support was not found at configure time; cannot build executable\n";
    return 1;
  }
  try {
    SourceManager sources;
    const auto id = sources.loadFile(inputPath.string());
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
    if (hasGenericFunctions(*file)) {
      std::cerr << "LLVM codegen for generic functions is not supported yet\n";
      return 1;
    }
    const ast::FnDecl* main = findMain(*file);
    if (main == nullptr || !isExecutableMain(*main)) {
      std::cerr << "executable build requires fn main(): i64\n";
      return 1;
    }
    LLVMCodeGen codegen;
    const std::string ir = codegen.emitModule(*file);
    TemporaryFile irFile(temporaryIrPath());
    {
      std::ofstream stream(irFile.path());
      if (!stream) {
        std::cerr << fmt::format("failed to write temporary IR file: {}\n", irFile.path().string());
        return 1;
      }
      stream << ir;
    }
    const char* clangEnv = std::getenv("GURA_CLANG");
    const std::string clang = clangEnv != nullptr ? clangEnv : "clang++";
    const std::string command = shellQuote(clang) + " " + shellQuote(irFile.path().string()) + " -o " + shellQuote(outputPath.string());
    const int status = std::system(command.c_str());
    if (status != 0) {
      std::cerr << fmt::format("executable link failed with status {}\n", status);
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

} // namespace gura
