#pragma once

#include <string>
#include <string_view>

namespace gura {

class Identifier {
public:
  Identifier() = default;
  explicit Identifier(std::string text) : text_(std::move(text)) {}

  [[nodiscard]] std::string_view text() const { return text_; }
  [[nodiscard]] bool empty() const { return text_.empty(); }

private:
  std::string text_;
};

} // namespace gura
