#pragma once

#include "token.hpp"
#include "grove/common/Optional.hpp"
#include <string>

namespace grove::io {

struct ParseError {
  ParseError(const Token& tok, std::string msg) : source_token{tok}, message{std::move(msg)} {
    //
  }
  ParseError(std::string msg) : message{std::move(msg)} {
    //
  }
  ParseError() = default;

  std::string with_context(std::string_view source_text, int ctx_amount) const;

  Optional<Token> source_token;
  std::string message;
};

}