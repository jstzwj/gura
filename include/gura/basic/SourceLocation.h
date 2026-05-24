#pragma once

#include <cstdint>

namespace gura {

struct SourceLocation {
  std::uint32_t offset = 0;
  std::uint32_t line = 1;
  std::uint32_t column = 1;

  [[nodiscard]] constexpr bool isValid() const { return line != 0 && column != 0; }
};

struct Span {
  SourceLocation begin;
  SourceLocation end;
};

} // namespace gura
