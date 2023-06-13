vec3 unpack_rgb(uint c) {
  const uint m = 0xff;
  vec3 result;
  result.r = float(c & m) / 255.0;
  result.g = float((c >> 8u) & m) / 255.0;
  result.b = float((c >> 16u) & m) / 255.0;
  return result;
}

void unpack_colors(in uvec4 data, out vec3 color0, out vec3 color1, out vec3 color2, out vec3 color3) {
  color0 = unpack_rgb(data.r);
  color1 = unpack_rgb(data.g);
  color2 = unpack_rgb(data.b);
  color3 = unpack_rgb(data.a);
}

vec3 compute_color(vec4 material_info, vec3 color0, vec3 color1, vec3 color2, vec3 color3) {
  vec3 col0 = mix(color0, color1, material_info.r);
  vec3 col1 = mix(color2, color3, material_info.g);
  return mix(col0, col1, material_info.b);
}

vec3 material1_color(vec4 material_info, uvec4 packed_color) {
  vec3 color0;
  vec3 color1;
  vec3 color2;
  vec3 color3;
  unpack_colors(packed_color, color0, color1, color2, color3);
  return compute_color(material_info, color0, color1, color2, color3);
}