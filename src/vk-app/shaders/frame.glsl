vec3 spherical_to_cartesian(vec2 v) {
  float st = sin(v.x);
  vec3 n = vec3(cos(v.y) * st, cos(v.x), sin(v.y) * st);
  return normalize(n);
}

vec2 cartesian_to_spherical(vec3 n) {
  return vec2(acos(n.y), atan(n.z, n.x));
}

mat3 make_coordinate_system_y(vec3 up) {
  vec3 x = vec3(1.0, 0.0, 0.0);
  if (abs(dot(x, up)) > 0.999) {
    x = vec3(0.0, 1.0, 0.0);
  }
  vec3 z = normalize(cross(up, x));
  x = cross(z, up);
  return mat3(x, up, z);
}
