vec4 pack_1u32_4fn(uint v) {
  float x = float(v & 0xffu);
  float y = float((v >> 8u) & 0xffu);
  float z = float((v >> 16u) & 0xffu);
  float w = float((v >> 24u) & 0xffu);
  return vec4(x, y, z, w) / 255.0;
}