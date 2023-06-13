#pragma once

#include "token.hpp"
#include <vector>

namespace grove::glsl {

enum class ScanError {
  None = 0,
  UnterminatedStringLiteral
};

struct ScanResult {
  ScanResult();
  bool success() const;

  std::vector<Token> tokens;
  ScanError maybe_error;
};

ScanResult scan(const char* source, int size);

}