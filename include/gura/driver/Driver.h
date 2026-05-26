#pragma once

#include <span>
#include <string_view>

namespace gura {

class Driver {
public:
  int run(std::span<const char* const> args);

private:
  int printVersion();
  int lexFile(std::string_view path);
  int parseFile(std::string_view path);
  int checkFile(std::string_view path);
  int emitLlvm(std::string_view path);
  int buildExecutable(std::span<const char* const> args);
};

} // namespace gura
