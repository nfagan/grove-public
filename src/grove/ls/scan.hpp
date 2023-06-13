#pragma once

#include "common.hpp"
#include <vector>
#include <string>

namespace grove::ls {

struct ScanError {
  std::string message;
};

struct ScanResult {
  std::vector<ScanError> errors;
  std::vector<Token> tokens;
};

ScanResult scan(const char* src, int64_t size);

}