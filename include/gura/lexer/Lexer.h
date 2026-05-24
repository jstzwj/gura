#pragma once

#include "gura/lexer/Token.h"

#include <string_view>
#include <vector>

namespace gura {

class Lexer {
public:
  explicit Lexer(std::string_view source);

  [[nodiscard]] std::vector<Token> lexAll();
  [[nodiscard]] Token next();

private:
  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] char peek(std::size_t lookahead = 0) const;
  char advance();
  bool match(char expected);
  void skipTrivia();

  Token makeToken(TokenKind kind, SourceLocation begin, std::size_t beginOffset);
  Token lexIdentifierOrKeyword();
  Token lexEscapedIdentifier();
  Token lexNumber();
  Token lexString();
  Token lexChar();

  std::string_view source_;
  std::size_t offset_ = 0;
  std::uint32_t line_ = 1;
  std::uint32_t column_ = 1;
};

} // namespace gura
