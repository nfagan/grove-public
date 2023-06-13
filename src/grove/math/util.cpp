#include "util.hpp"
#include <cmath>
#include <cstdint>

int grove::next_pow2(int value) {
  return int(uint32_t(1) << uint32_t(std::ceil(std::log2(double(value)))));
}
