float linear_to_srgb(float c) {
  if (c <= 0.0031308) {
    return c * 12.92;
  } else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec3 linear_to_srgb(vec3 c) {
  c.x = linear_to_srgb(c.x);
  c.y = linear_to_srgb(c.y);
  c.z = linear_to_srgb(c.z);
  return c;
}