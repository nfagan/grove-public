#include "token.hpp"
#include "grove/common/common.hpp"
#include <cassert>
#include <iostream>

GROVE_NAMESPACE_BEGIN

void io::Token::show() const {
  std::cout << io::to_string(type) << ": " << lexeme << std::endl;
}

std::string io::Token::to_string() const {
  return std::string{io::to_string(type)} + ": " + std::string{lexeme};
}

const io::Token& io::Token::null() {
  static Token tok{};
  return tok;
}

const char* io::to_string(TokenType type) {
  switch (type) {
    case TokenType::Null:
      return "Null";
    case TokenType::Colon:
      return "Colon";
    case TokenType::Comma:
      return "Comma";
    case TokenType::Tab:
      return "Tab";
    case TokenType::Newline:
      return "Newline";
    case TokenType::Apostrophe:
      return "Apostrophe";
    case TokenType::Period:
      return "Period";
    case TokenType::LeftBracket:
      return "LeftBracket";
    case TokenType::RightBracket:
      return "RightBracket";
    case TokenType::LeftBrace:
      return "LeftBrace";
    case TokenType::RightBrace:
      return "RightBrace";
    case TokenType::Number:
      return "Number";
    case TokenType::String:
      return "String";
    case TokenType::Identifier:
      return "Identifier";
    case TokenType::KeywordNew:
      return "KeywordNew";
    case TokenType::KeywordRef:
      return "KeywordRef";
    default:
      assert(false);
      return "";
  }
}

GROVE_NAMESPACE_END
