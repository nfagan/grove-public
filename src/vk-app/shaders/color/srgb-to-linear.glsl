float srgb_to_linear(float c) {
  if (c <= 0.0404482362771082) {
    return c / 12.92;
  } else {
    return pow((c + 0.055) / 1.055, 2.4);
  }
}

vec3 srgb_to_linear(vec3 c) {
  c.x = srgb_to_linear(c.x);
  c.y = srgb_to_linear(c.y);
  c.z = srgb_to_linear(c.z);
  return c;
}