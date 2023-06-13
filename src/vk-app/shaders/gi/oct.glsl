float sign_not_zero1(float a) {
  return a >= 0.0 ? 1.0 : -1.0;
}

vec2 sign_not_zero2(vec2 v) {
  return vec2(sign_not_zero1(v.x), sign_not_zero1(v.y));
}

vec2 tetra_encode(vec3 v) {
  return v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
}

vec2 oct_encode(vec3 v) {
  vec2 p = tetra_encode(v);
  if (v.z <= 0.0) {
    p = (1.0 - abs(p.yx)) * sign_not_zero2(p);
  }
  return p;
}

vec3 oct_decode(vec2 p) {
  vec3 v = vec3(p.xy, 1.0 - abs(p.x) - abs(p.y));
  if (v.z < 0.0) {
    v.xy = (1.0 - abs(v.yx)) * sign_not_zero2(v.xy);
  }
  return normalize(v);
}