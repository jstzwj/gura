#pragma once

#include "gura/basic/SourceLocation.h"

#include <string>
#include <string_view>

namespace gura {

enum class TokenKind {
  EndOfFile,
  Invalid,
  Identifier,
  IntegerLiteral,
  FloatLiteral,
  StringLiteral,
  CharLiteral,

  KwFn,
  KwStruct,
  KwEnum,
  KwTrait,
  KwImpl,
  KwLet,
  KwVar,
  KwMove,
  KwNew,
  KwEnter,
  KwExplore,
  KwFreeze,
  KwMerge,
  KwCown,
  KwAcquire,
  KwSpawn,
  KwUnsafe,
  KwReturn,
  KwIf,
  KwElse,
  KwWhile,
  KwMatch,
  KwCase,
  KwAs,
  KwTrue,
  KwFalse,
  KwNone,
  KwMut,
  KwTmp,
  KwIso,
  KwImm,
  KwPau,

  LParen,
  RParen,
  LBrace,
  RBrace,
  LBracket,
  RBracket,
  Comma,
  Dot,
  Colon,
  Semicolon,
  Arrow,
  FatArrow,
  Question,
  Pipe,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Equal,
  EqualEqual,
  Bang,
  BangEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
};

struct Token {
  TokenKind kind = TokenKind::Invalid;
  Span span;
  std::string text;
};

[[nodiscard]] std::string_view tokenKindName(TokenKind kind);

} // namespace gura
