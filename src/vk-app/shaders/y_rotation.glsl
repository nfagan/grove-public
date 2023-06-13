mat3 y_rotation(float theta) {
  mat3 m = mat3(1.0);
  float st = sin(theta);
  float ct = cos(theta);

  m[0][0] = ct;
  m[2][0] = st;
  m[0][2] = -st;
  m[2][2] = ct;

  return m;
}
