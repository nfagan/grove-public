vec2 spherical_to_uv(vec2 c) {
  float theta = c.x;
  float phi = c.y;
  float min_phi = -PI;
  float max_phi = PI;
  float min_theta = 0.0;
  float max_theta = PI;
  float u = (max(phi, min_phi) - min_phi) / (max_phi - min_phi);
  float v = max(min_theta, min(max_theta, theta)) / PI;
  return vec2(u, v);
}
