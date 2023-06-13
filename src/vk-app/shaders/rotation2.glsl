mat2 rotation2(float theta) {
  float ct = cos(theta);
  float st = sin(theta);
  vec2 c0 = vec2(ct, st);
  vec2 c1 = vec2(-st, ct);
  return mat2(c0, c1);
}
