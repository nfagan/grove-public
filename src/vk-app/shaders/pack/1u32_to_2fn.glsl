vec2 pack_1u32_2fn(uint v) {
  uint mask = 0xffffu;
  uint b16 = v & mask;
  uint a16 = (v & ~mask) >> 16u;
  return vec2(float(a16), float(b16)) / float(mask);
}
