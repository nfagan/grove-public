uvec4 pack_1u32_4u8(uint v) {
  uint x = v & 0xffu;
  uint y = (v >> 8u) & 0xffu;
  uint z = (v >> 16u) & 0xffu;
  uint w = (v >> 24u) & 0xffu;
  return uvec4(x, y, z, w);
}
