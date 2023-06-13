#pragma once

#include "ast.hpp"
#include <vector>

namespace grove::glsl {

enum class ParseError {
  None = 0,
  UnexpectedTokenType
};

struct ParseResult {
  ParseResult();
  bool success() const;

  ParseError maybe_error;
  std::vector<BoxedAstNode> nodes;
};

ParseResult parse(const std::vector<Token>& tokens);

}