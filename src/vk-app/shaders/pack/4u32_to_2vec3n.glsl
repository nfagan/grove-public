void pack_4u32_to_2vec3n(in uvec4 data, out vec3 a, out vec3 b) {
  const uint m = 0xffffu;
  // .x = (u16) [p0.x, p0.y], .y = [p0.z, p1.x], .z = [p1.y, p1.z], .w = <unused>
  a = vec3(
    float(data.x & m),
    float((data.x >> 16u) & m),
    float(data.y & m)) / float(m);
  b = vec3(
    float((data.y >> 16u) & m),
    float(data.z & m),
    float((data.z >> 16u) & m)) / float(m);
}