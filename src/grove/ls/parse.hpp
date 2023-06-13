#pragma once

#include "common.hpp"
#include "StringRegistry.hpp"
#include <string>
#include <vector>

namespace grove::ls {

struct ParseError {
  std::string message;
  uint32_t token;
};

struct ParseResult {
  std::vector<ParseError> errors;
  std::vector<AstNode> nodes;
  std::vector<uint32_t> parameters;
  std::vector<uint32_t> subscripts;
  std::vector<uint32_t> statement_blocks;
  std::vector<uint32_t> module_strings;
  std::vector<uint32_t> rules;
  std::vector<uint32_t> systems;
  std::vector<uint32_t> modules;
  std::vector<uint32_t> axioms;
  std::vector<uint32_t> module_meta_type_labels;
};

struct ParseParams {
  StringRegistry* str_registry;
  const char* source;
};

ParseResult parse(const Token* tokens, int64_t size, const ParseParams* params);

}