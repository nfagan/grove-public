mat3 x_rotation(float theta) {
  mat3 m = mat3(1.0);
  float st = sin(theta);
  float ct = cos(theta);

  m[1][1] = ct;
  m[1][2] = st;
  m[2][1] = -st;
  m[2][2] = ct;

  return m;
}