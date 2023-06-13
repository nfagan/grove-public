#pragma once

#include <string_view>

namespace grove::glsl {

enum class TokenType {
  Null = 0,
  Pound,
  Identifier,
  StringLiteral,
  KeywordPragma,
  KeywordInclude,
  NewLine
};

struct Token {
  TokenType type;
  std::string_view lexeme;

  static const Token null_token;
};

}