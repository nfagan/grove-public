#version 450

//  vertices
layout (location = 0) in vec2 position;
//  dynamic
layout (location = 1) in uvec4 directions0;
layout (location = 2) in uvec4 directions1;
layout (location = 3) in vec4 instance_position_radius;
layout (location = 4) in vec4 child_instance_position_radius;
//  wind
layout (location = 5) in uvec4 packed_axis_root_info0;
layout (location = 6) in uvec4 packed_axis_root_info1;
layout (location = 7) in uvec4 packed_axis_root_info2;

#pragma include "pi.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "proc-tree/common.glsl"
#pragma include "proc-tree/roots-wind-data.glsl"
#pragma include "proc-tree/roots-common.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/roots-wind-displacement.glsl"

layout (location = 0) out VS_OUT vs_out;

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

float wind_attenuation(float wind_strength) {
  const vec2 wind_displacement_limits = vec2(0.1, 0.3);
  const vec2 wind_strength_limits = vec2(0.03, 0.1);
  float a = smoothstep(wind_displacement_limits.x, wind_displacement_limits.y, wind_strength);
  return mix(wind_strength_limits.x, wind_strength_limits.y, a);
}

vec2 num_points_xz() {
  uint ps = num_points_xz_color_sun_position_xy.x;
  return vec2(float(ps & 0xffffu), float((ps >> 16u) & 0xffffu));
}

void main() {
  vec3 instance_position = instance_position_radius.xyz;
  vec3 child_instance_position = child_instance_position_radius.xyz;
  float self_radius = instance_position_radius.w;
  float child_radius = child_instance_position_radius.w;

  vec3 aabb_p0 = aabb_p0_t.xyz;
  float t = aabb_p0_t.w;
  vec3 aabb_p1 = aabb_p1_wind_strength.xyz;
  float wind_strength = aabb_p1_wind_strength.w;

  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(packed_axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(packed_axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(packed_axis_root_info2, root_info2, child_root_info2);

  vec3 s = shape_function(num_points_xz());
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  vec3 s_base = s0 * instance_scale(self_radius);
  vec3 s_tip = s0 * instance_scale(child_radius);

  mat3 base_coord_sys;
  mat3 tip_coord_sys;
  unpack_coord_sys(base_coord_sys, tip_coord_sys);

  vec3 s0n = normalize(s0);
  vec3 n_base = normalize(base_coord_sys * s0n);
  vec3 n_tip = normalize(tip_coord_sys * s0n);
  vec3 n = mix(n_base, n_tip, y);

  if (length(n) == 0.0) {
    n = vec3(0.0, 1.0, 0.0);
  }

  vec3 p_base = base_coord_sys * s_base + instance_position;
  vec3 p_tip = tip_coord_sys * s_tip + child_instance_position;

//  float wind_atten = 0.5 * wind_strength;
  float wind_mask = wind_strength == 0.0 ? 0.0 : 1.0;
  float wind_atten = wind_mask * 0.5 * wind_attenuation(wind_strength);
  p_base = wind_displacement(p_base, t, aabb_p0, aabb_p1, root_info0, root_info1, root_info2, wind_atten);
  p_tip = wind_displacement(p_tip, t, aabb_p0, aabb_p1, child_root_info0, child_root_info1, child_root_info2, wind_atten);
  vec3 p = mix(p_base, p_tip, y);

  vs_out.normal = n;

  gl_Position = projection_view * vec4(p, 1.0);
}