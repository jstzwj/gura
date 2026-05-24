#include "gura/lexer/Lexer.h"

#include <cctype>
#include <unordered_map>

namespace gura {

namespace {

bool isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentifierContinue(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

TokenKind keywordKind(std::string_view text) {
  static const std::unordered_map<std::string_view, TokenKind> keywords = {
      {"fn", TokenKind::KwFn},           {"struct", TokenKind::KwStruct}, {"enum", TokenKind::KwEnum},
      {"trait", TokenKind::KwTrait},     {"impl", TokenKind::KwImpl},     {"let", TokenKind::KwLet},
      {"var", TokenKind::KwVar},         {"move", TokenKind::KwMove},     {"new", TokenKind::KwNew},
      {"enter", TokenKind::KwEnter},     {"explore", TokenKind::KwExplore}, {"freeze", TokenKind::KwFreeze},
      {"merge", TokenKind::KwMerge},     {"cown", TokenKind::KwCown},     {"acquire", TokenKind::KwAcquire},
      {"spawn", TokenKind::KwSpawn},     {"unsafe", TokenKind::KwUnsafe}, {"return", TokenKind::KwReturn},
      {"if", TokenKind::KwIf},           {"else", TokenKind::KwElse},     {"while", TokenKind::KwWhile},
      {"match", TokenKind::KwMatch},
      {"case", TokenKind::KwCase},       {"as", TokenKind::KwAs},         {"true", TokenKind::KwTrue},
      {"false", TokenKind::KwFalse},
      {"none", TokenKind::KwNone},       {"mut", TokenKind::KwMut},       {"tmp", TokenKind::KwTmp},
      {"iso", TokenKind::KwIso},         {"imm", TokenKind::KwImm},       {"pau", TokenKind::KwPau},
  };
  if (auto it = keywords.find(text); it != keywords.end()) {
    return it->second;
  }
  return TokenKind::Identifier;
}

} // namespace

std::string_view tokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::EndOfFile: return "eof";
  case TokenKind::Invalid: return "invalid";
  case TokenKind::Identifier: return "identifier";
  case TokenKind::IntegerLiteral: return "integer_literal";
  case TokenKind::FloatLiteral: return "float_literal";
  case TokenKind::StringLiteral: return "string_literal";
  case TokenKind::CharLiteral: return "char_literal";
  case TokenKind::KwFn: return "fn";
  case TokenKind::KwStruct: return "struct";
  case TokenKind::KwEnum: return "enum";
  case TokenKind::KwTrait: return "trait";
  case TokenKind::KwImpl: return "impl";
  case TokenKind::KwLet: return "let";
  case TokenKind::KwVar: return "var";
  case TokenKind::KwMove: return "move";
  case TokenKind::KwNew: return "new";
  case TokenKind::KwEnter: return "enter";
  case TokenKind::KwExplore: return "explore";
  case TokenKind::KwFreeze: return "freeze";
  case TokenKind::KwMerge: return "merge";
  case TokenKind::KwCown: return "cown";
  case TokenKind::KwAcquire: return "acquire";
  case TokenKind::KwSpawn: return "spawn";
  case TokenKind::KwUnsafe: return "unsafe";
  case TokenKind::KwReturn: return "return";
  case TokenKind::KwIf: return "if";
  case TokenKind::KwElse: return "else";
  case TokenKind::KwWhile: return "while";
  case TokenKind::KwMatch: return "match";
  case TokenKind::KwCase: return "case";
  case TokenKind::KwAs: return "as";
  case TokenKind::KwTrue: return "true";
  case TokenKind::KwFalse: return "false";
  case TokenKind::KwNone: return "none";
  case TokenKind::KwMut: return "mut";
  case TokenKind::KwTmp: return "tmp";
  case TokenKind::KwIso: return "iso";
  case TokenKind::KwImm: return "imm";
  case TokenKind::KwPau: return "pau";
  case TokenKind::LParen: return "(";
  case TokenKind::RParen: return ")";
  case TokenKind::LBrace: return "{";
  case TokenKind::RBrace: return "}";
  case TokenKind::LBracket: return "[";
  case TokenKind::RBracket: return "]";
  case TokenKind::Comma: return ",";
  case TokenKind::Dot: return ".";
  case TokenKind::Colon: return ":";
  case TokenKind::Semicolon: return ";";
  case TokenKind::Arrow: return "->";
  case TokenKind::FatArrow: return "=>";
  case TokenKind::Question: return "?";
  case TokenKind::Pipe: return "|";
  case TokenKind::Plus: return "+";
  case TokenKind::Minus: return "-";
  case TokenKind::Star: return "*";
  case TokenKind::Slash: return "/";
  case TokenKind::Percent: return "%";
  case TokenKind::Equal: return "=";
  case TokenKind::EqualEqual: return "==";
  case TokenKind::Bang: return "!";
  case TokenKind::BangEqual: return "!=";
  case TokenKind::Less: return "<";
  case TokenKind::LessEqual: return "<=";
  case TokenKind::Greater: return ">";
  case TokenKind::GreaterEqual: return ">=";
  }
  return "unknown";
}

Lexer::Lexer(std::string_view source) : source_(source) {}

std::vector<Token> Lexer::lexAll() {
  std::vector<Token> tokens;
  do {
    tokens.push_back(next());
  } while (tokens.back().kind != TokenKind::EndOfFile);
  return tokens;
}

Token Lexer::next() {
  skipTrivia();
  const SourceLocation begin{static_cast<std::uint32_t>(offset_), line_, column_};
  const std::size_t beginOffset = offset_;

  if (isAtEnd()) {
    return Token{TokenKind::EndOfFile, Span{begin, begin}, ""};
  }

  const char c = advance();
  if (isIdentifierStart(c)) {
    return lexIdentifierOrKeyword();
  }
  if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
    return lexNumber();
  }
  if (c == '`') {
    return lexEscapedIdentifier();
  }
  if (c == '"') {
    return lexString();
  }
  if (c == '\'') {
    return lexChar();
  }

  switch (c) {
  case '(': return makeToken(TokenKind::LParen, begin, beginOffset);
  case ')': return makeToken(TokenKind::RParen, begin, beginOffset);
  case '{': return makeToken(TokenKind::LBrace, begin, beginOffset);
  case '}': return makeToken(TokenKind::RBrace, begin, beginOffset);
  case '[': return makeToken(TokenKind::LBracket, begin, beginOffset);
  case ']': return makeToken(TokenKind::RBracket, begin, beginOffset);
  case ',': return makeToken(TokenKind::Comma, begin, beginOffset);
  case '.': return makeToken(TokenKind::Dot, begin, beginOffset);
  case ':': return makeToken(TokenKind::Colon, begin, beginOffset);
  case ';': return makeToken(TokenKind::Semicolon, begin, beginOffset);
  case '?': return makeToken(TokenKind::Question, begin, beginOffset);
  case '|': return makeToken(TokenKind::Pipe, begin, beginOffset);
  case '+': return makeToken(TokenKind::Plus, begin, beginOffset);
  case '-': return makeToken(match('>') ? TokenKind::Arrow : TokenKind::Minus, begin, beginOffset);
  case '*': return makeToken(TokenKind::Star, begin, beginOffset);
  case '/': return makeToken(TokenKind::Slash, begin, beginOffset);
  case '%': return makeToken(TokenKind::Percent, begin, beginOffset);
  case '=': return makeToken(match('=') ? TokenKind::EqualEqual : (match('>') ? TokenKind::FatArrow : TokenKind::Equal), begin, beginOffset);
  case '!': return makeToken(match('=') ? TokenKind::BangEqual : TokenKind::Bang, begin, beginOffset);
  case '<': return makeToken(match('=') ? TokenKind::LessEqual : TokenKind::Less, begin, beginOffset);
  case '>': return makeToken(match('=') ? TokenKind::GreaterEqual : TokenKind::Greater, begin, beginOffset);
  default: return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
}

bool Lexer::isAtEnd() const {
  return offset_ >= source_.size();
}

char Lexer::peek(std::size_t lookahead) const {
  if (offset_ + lookahead >= source_.size()) {
    return '\0';
  }
  return source_[offset_ + lookahead];
}

char Lexer::advance() {
  const char c = source_[offset_++];
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (isAtEnd() || source_[offset_] != expected) {
    return false;
  }
  advance();
  return true;
}

void Lexer::skipTrivia() {
  for (;;) {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
      advance();
    }
    if (peek() == '/' && peek(1) == '/') {
      while (!isAtEnd() && peek() != '\n') {
        advance();
      }
      continue;
    }
    if (peek() == '/' && peek(1) == '*') {
      advance();
      advance();
      while (!isAtEnd() && !(peek() == '*' && peek(1) == '/')) {
        advance();
      }
      if (!isAtEnd()) {
        advance();
        advance();
      }
      continue;
    }
    break;
  }
}

Token Lexer::makeToken(TokenKind kind, SourceLocation begin, std::size_t beginOffset) {
  const SourceLocation end{static_cast<std::uint32_t>(offset_), line_, column_};
  return Token{kind, Span{begin, end}, std::string(source_.substr(beginOffset, offset_ - beginOffset))};
}

Token Lexer::lexIdentifierOrKeyword() {
  const auto beginOffset = offset_ - 1;
  const SourceLocation begin{static_cast<std::uint32_t>(beginOffset), line_, static_cast<std::uint32_t>(column_ - 1)};
  while (isIdentifierContinue(peek())) {
    advance();
  }
  const auto text = source_.substr(beginOffset, offset_ - beginOffset);
  return Token{keywordKind(text), Span{begin, SourceLocation{static_cast<std::uint32_t>(offset_), line_, column_}}, std::string(text)};
}

Token Lexer::lexEscapedIdentifier() {
  const auto beginOffset = offset_ - 1;
  const SourceLocation begin{static_cast<std::uint32_t>(beginOffset), line_, static_cast<std::uint32_t>(column_ - 1)};
  while (!isAtEnd() && peek() != '`' && peek() != '\n') {
    advance();
  }
  if (!match('`')) {
    return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
  return Token{TokenKind::Identifier, Span{begin, SourceLocation{static_cast<std::uint32_t>(offset_), line_, column_}}, std::string(source_.substr(beginOffset + 1, offset_ - beginOffset - 2))};
}

Token Lexer::lexNumber() {
  const auto beginOffset = offset_ - 1;
  const SourceLocation begin{static_cast<std::uint32_t>(beginOffset), line_, static_cast<std::uint32_t>(column_ - 1)};
  while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
    advance();
  }
  TokenKind kind = TokenKind::IntegerLiteral;
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1))) != 0) {
    kind = TokenKind::FloatLiteral;
    advance();
    while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
      advance();
    }
  }
  if (peek() == 'i' || peek() == 'f') {
    const char suffixKind = peek();
    advance();
    if ((peek() == '3' && peek(1) == '2') || (peek() == '6' && peek(1) == '4')) {
      advance();
      advance();
      if (isIdentifierContinue(peek()) || (kind == TokenKind::IntegerLiteral && suffixKind == 'f') || (kind == TokenKind::FloatLiteral && suffixKind == 'i')) {
        while (isIdentifierContinue(peek())) {
          advance();
        }
        return makeToken(TokenKind::Invalid, begin, beginOffset);
      }
      return makeToken(kind, begin, beginOffset);
    }
    while (isIdentifierContinue(peek())) {
      advance();
    }
    return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
  if (isIdentifierStart(peek())) {
    while (isIdentifierContinue(peek())) {
      advance();
    }
    return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
  return makeToken(kind, begin, beginOffset);
}

Token Lexer::lexString() {
  const auto beginOffset = offset_ - 1;
  const SourceLocation begin{static_cast<std::uint32_t>(beginOffset), line_, static_cast<std::uint32_t>(column_ - 1)};
  if (peek() == '"' && peek(1) == '"') {
    advance();
    advance();
    while (!isAtEnd() && !(peek() == '"' && peek(1) == '"' && peek(2) == '"')) {
      advance();
    }
    if (!isAtEnd()) {
      advance();
      advance();
      advance();
    }
    return makeToken(TokenKind::StringLiteral, begin, beginOffset);
  }
  while (!isAtEnd() && peek() != '"' && peek() != '\n') {
    if (peek() == '\\' && peek(1) != '\0') {
      advance();
    }
    advance();
  }
  if (!match('"')) {
    return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
  return makeToken(TokenKind::StringLiteral, begin, beginOffset);
}

Token Lexer::lexChar() {
  const auto beginOffset = offset_ - 1;
  const SourceLocation begin{static_cast<std::uint32_t>(beginOffset), line_, static_cast<std::uint32_t>(column_ - 1)};
  if (!isAtEnd()) {
    if (peek() == '\\') {
      advance();
    }
    advance();
  }
  if (!match('\'')) {
    return makeToken(TokenKind::Invalid, begin, beginOffset);
  }
  return makeToken(TokenKind::CharLiteral, begin, beginOffset);
}

} // namespace gura
