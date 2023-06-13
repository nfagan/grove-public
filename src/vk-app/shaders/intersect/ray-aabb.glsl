bool ray_aabb_intersect(vec3 ro, vec3 rd, vec3 p0, vec3 p1, out float t0, out float t1) {
  const float large_number = 3.3e30;

  float tmp_t0 = -large_number;
  float tmp_t1 = large_number;

  for (int i = 0; i < 3; i++) {
    float inv_d = 1.0 / rd[i];
    float t00 = (p0[i] - ro[i]) * inv_d;
    float t11 = (p1[i] - ro[i]) * inv_d;

    if (t00 > t11) {
      float tmp = t00;
      t00 = t11;
      t11 = tmp;
    }

    tmp_t0 = max(tmp_t0, t00);
    tmp_t1 = min(tmp_t1, t11);

    if (tmp_t0 > tmp_t1) {
      return false;
    }
  }

  t0 = tmp_t0;
  t1 = tmp_t1;

  return true;
}