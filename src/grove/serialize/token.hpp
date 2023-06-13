#pragma once

#include <string_view>

namespace grove::io {

using Lexeme = std::string_view;

enum class TokenType {
  Null = 0,
  Colon,
  Comma,
  Tab,
  Newline,
  Apostrophe,
  Period,
  LeftBracket,
  RightBracket,
  LeftBrace,
  RightBrace,
  Number,
  String,
  Identifier,
  KeywordNew,
  KeywordRef,
};

struct Token {
  void show() const;
  std::string to_string() const;
  static const Token& null();

  TokenType type{};
  Lexeme lexeme{};
};

const char* to_string(TokenType type);

}