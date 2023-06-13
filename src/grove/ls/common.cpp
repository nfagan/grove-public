#include "common.hpp"
#include "grove/common/common.hpp"
#include <cassert>
#include <iostream>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace ls;

} //  anon

const char* ls::to_string(TokenType type) {
  switch (type) {
    case TokenType::Null:
      return "Null";
    case TokenType::Number:
      return "Number";
    case TokenType::Identifier:
      return "Identifier";
    case TokenType::Lparen:
      return "Lparen";
    case TokenType::Rparen:
      return "Rparen";
    case TokenType::Lbracket:
      return "Lbracket";
    case TokenType::Rbracket:
      return "Rbracket";
    case TokenType::Lbrace:
      return "Lbrace";
    case TokenType::Rbrace:
      return "Rbrace";
    case TokenType::Colon:
      return "Colon";
    case TokenType::Arrow:
      return "Arrow";
    case TokenType::Plus:
      return "Plus";
    case TokenType::Minus:
      return "Minus";
    case TokenType::Lt:
      return "Lt";
    case TokenType::Le:
      return "Le";
    case TokenType::Gt:
      return "Gt";
    case TokenType::Ge:
      return "Ge";
    case TokenType::Asterisk:
      return "Asterisk";
    case TokenType::Fslash:
      return "Fslash";
    case TokenType::Bslash:
      return "Bslash";
    case TokenType::Comma:
      return "Comma";
    case TokenType::Period:
      return "Period";
    case TokenType::Define:
      return "Define";
    case TokenType::Equal:
      return "Equal";
    case TokenType::EqualEqual:
      return "EqualEqual";
    case TokenType::KwModule:
      return "KwModule";
    case TokenType::KwSystem:
      return "KwSystem";
    case TokenType::KwRule:
      return "KwRule";
    case TokenType::KwEnd:
      return "KwEnd";
    case TokenType::KwPred:
      return "KwPred";
    case TokenType::KwIf:
      return "KwIf";
    case TokenType::KwElse:
      return "KwElse";
    case TokenType::KwReturn:
      return "KwReturn";
    case TokenType::KwMatch:
      return "KwMatch";
    case TokenType::KwAxiom:
      return "KwAxiom";
    case TokenType::KwIs:
      return "KwIs";
    default:
      assert(false);
      return "";
  }
}

std::string_view ls::make_lexeme(const Token& tok, const char* src) {
  return std::string_view{src + tok.begin, tok.end - tok.begin};
}

void ls::show(const Token& tok, const char* src) {
  std::cout << to_string(tok.type) << ": ";
  for (int64_t i = tok.begin; i < tok.end; i++) {
    std::cout << src[i];
  }
  std::cout << std::endl;
}

GROVE_NAMESPACE_END