#pragma once

#include "token.hpp"
#include "error.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>

namespace grove::io {

class StringRegistry;

struct ParseResult {
  ast::Ast ast;
  std::vector<ParseError> errors;
  bool success{};
};

struct ParseInfo {
  StringRegistry& string_registry;
};

ParseResult parse(const std::vector<Token>& tokens, ParseInfo& parse_info);

}