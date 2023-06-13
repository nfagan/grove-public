vec3 unpack_rgb_(uint c) {
  const uint m = 0xff;
  vec3 result;
  result.r = float(c & m) / 255.0;
  result.g = float((c >> 8u) & m) / 255.0;
  result.b = float((c >> 16u) & m) / 255.0;
  return result;
}

vec3 material1_color(vec4 material_info, uvec4 packed_colors) {
  vec3 color0 = unpack_rgb_(packed_colors.r);
  vec3 color1 = unpack_rgb_(packed_colors.g);
  vec3 color2 = unpack_rgb_(packed_colors.b);
  vec3 color3 = unpack_rgb_(packed_colors.a);
  vec3 col0 = mix(color0, color1, material_info.r);
  vec3 col1 = mix(color2, color3, material_info.g);
  return mix(col0, col1, material_info.b);
}