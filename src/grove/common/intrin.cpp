#include "intrin.hpp"
#include "grove/common/common.hpp"

#ifdef _MSC_VER
#include <immintrin.h>
#endif

GROVE_NAMESPACE_BEGIN

#ifdef _MSC_VER

uint64_t ctzll(uint64_t v) {
  return v == 0 ? 64 : _tzcnt_u64(v);
}

#else

uint64_t ctzll(uint64_t v) {
  return v == 0 ? 64 : __builtin_ctzll(v);
}

#endif

uint64_t ffsll_one_based(uint64_t v) {
  auto lz = ctzll(v);
  return lz == 64 ? 0 : lz + 1;
}

GROVE_NAMESPACE_END