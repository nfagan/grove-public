//  Requires x_rotation.glsl

vec3 axis_root_position_to_world(vec3 root_p, vec3 aabb_p0, vec3 aabb_p1) {
  vec3 span = aabb_p1 - aabb_p0;
  return span * root_p + aabb_p0;
}

vec3 axis_rotation(vec3 p, vec3 root, float theta) {
  vec3 p_off = p - root;
  mat3 m = x_rotation(theta); //  @NOTE - external dependency
  p_off = m * p_off + root;
#if 1
  return p_off;
#else
  vec3 p_disp = p_off - p;
  float len = max(0.001, length(p_disp));
  return p + (p_disp / len) * min(len, 0.1);
#endif
}

vec2 xz_origin_relative_position(vec3 p, vec3 aabb_p0, vec3 aabb_p1) {
  vec2 xz_span = (aabb_p1 - aabb_p0).xz;
  vec2 xz_min = aabb_p0.xz;
  vec2 xz01 = (p.xz - xz_min) / xz_span;
  return xz01 * 2.0 - 1.0;
}

float y_position01(vec3 p, float y0, float y1) {
  return (p.y - y0) / (y1 - y0);
}

#if 1
vec3 wind_displacement(vec3 p, float t, vec3 aabb_p0, vec3 aabb_p1,
                       vec4 root_info0, vec4 root_info1, vec4 root_info2, float wind_atten) {
  float theta_span = PI / 32.0;

  float y_frac = y_position01(p, aabb_p0.y, aabb_p1.y);
  vec2 xz_ori_relative = abs(xz_origin_relative_position(p, aabb_p0, aabb_p1));

  float xz_ori_rel = (xz_ori_relative.x + xz_ori_relative.y) * 0.5;
  float xz_atten = pow(xz_ori_rel, 4.0);
  float y_atten = pow(y_frac, 2.0);
  float atten = clamp(xz_atten + y_atten, 0.0, 1.0) * wind_atten;

  float is_active2 = root_info2.w;
  if (is_active2 >= 0.5) {
    vec3 root_p = root_info2.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t * 8.0 + length(root_p)) * theta_span * atten);
  }

  float is_active1 = root_info1.w;
  if (is_active1 >= 0.5) {
    vec3 root_p = root_info1.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t * 4.0 + length(root_p)) * theta_span * atten);
  }

  float is_active0 = root_info0.w;
  if (is_active0 >= 0.5) {
    vec3 root_p = root_info0.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t + length(root_p)) * theta_span * atten);
  }

  return p;
}
#else
vec3 wind_displacement(vec3 p, float t, vec3 aabb_p0, vec3 aabb_p1,
                       vec4 root_info0, vec4 root_info1, vec4 root_info2, float wind_atten) {
  float y_frac = y_position01(p, aabb_p0.y, aabb_p1.y);
  vec2 xz_ori_relative = abs(xz_origin_relative_position(p, aabb_p0, aabb_p1));

  float xz_ori_rel = (xz_ori_relative.x + xz_ori_relative.y) * 0.5;
  float xz_atten = max(0.25, pow(xz_ori_rel, 4.0));
  float y_atten = pow(y_frac, 2.0);
  float atten = clamp(xz_atten, 0.0, 1.0) * wind_atten;

  float is_active2 = root_info2.w;
  if (is_active2 >= 0.5) {
    float rot_span = PI / 8.0;
    vec3 root_p = root_info2.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t * 8.0 + length(root_p)) * rot_span * atten);
  }

  float is_active1 = root_info1.w;
  if (is_active1 >= 0.5) {
    float rot_span = PI / 16.0;
    vec3 root_p = root_info1.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t * 4.0 + length(root_p)) * rot_span * atten);
  }

  float is_active0 = root_info0.w;
  if (is_active0 >= 0.5) {
    float rot_span = PI / 32.0;
    vec3 root_p = root_info0.xyz;
    root_p = axis_root_position_to_world(root_p, aabb_p0, aabb_p1);
    p = axis_rotation(p, root_p, sin(t + length(root_p)) * rot_span * atten);
  }

  return p;
}
#endif