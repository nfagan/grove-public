#pragma once

#include "token.hpp"
#include "error.hpp"
#include <vector>

namespace grove::io {

struct ScanResult {
  bool success{};
  std::vector<Token> tokens;
  std::vector<ParseError> errors;
};

ScanResult scan(const char* text, std::size_t size);

}