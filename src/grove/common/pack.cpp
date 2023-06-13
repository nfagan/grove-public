#include "pack.hpp"
#include "common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

constexpr uint16_t u16_max() {
  return uint16_t(~uint16_t(0));
}

} //  anon

uint32_t pack::pack_2fn_1u32(float a, float b) {
  assert(a >= 0.0f && a <= 1.0f);
  assert(b >= 0.0f && b <= 1.0f);
  auto a32 = uint32_t(a * float(u16_max()));
  auto b32 = uint32_t(b * float(u16_max()));
  a32 <<= 16u;
  a32 |= b32;
  return a32;
}

void pack::unpack_1u32_2fn(uint32_t v, float* af, float* bf) {
  auto mask = uint32_t(u16_max());
  auto b16 = uint16_t(v & mask);
  auto a16 = uint16_t((v & ~mask) >> 16u);
  *af = float(a16) / float(u16_max());
  *bf = float(b16) / float(u16_max());
}

uint32_t pack::pack_4u8_1u32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint32_t r{};
  r |= uint32_t(a);
  r |= uint32_t(uint32_t(b) << 8u);
  r |= uint32_t(uint32_t(c) << 16u);
  r |= uint32_t(uint32_t(d) << 24u);
  return r;
}

void pack::unpack_1u32_4u8(uint32_t v, uint8_t* a, uint8_t* b, uint8_t* c, uint8_t* d) {
  const uint32_t m = 0xff;
  *a = v & m;
  *b = (v >> 8u) & m;
  *c = (v >> 16u) & m;
  *d = (v >> 24u) & m;
}

GROVE_NAMESPACE_END

