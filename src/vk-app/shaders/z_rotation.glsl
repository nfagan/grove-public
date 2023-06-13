mat3 z_rotation(float theta) {
  mat3 m = mat3(1.0);
  float st = sin(theta);
  float ct = cos(theta);

  m[0][0] = ct;
  m[0][1] = st;
  m[1][0] = -st;
  m[1][1] = ct;

  return m;
}
