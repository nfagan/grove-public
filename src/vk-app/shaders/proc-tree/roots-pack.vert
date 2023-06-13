#version 450

//  vertices
layout (location = 0) in vec2 position;
//  dynamic
layout (location = 1) in uvec4 directions0;
layout (location = 2) in uvec4 directions1;
layout (location = 3) in vec4 instance_position_radius;
layout (location = 4) in vec4 child_instance_position_radius;

#pragma include "pi.glsl"
#pragma include "proc-tree/common.glsl"
#pragma include "proc-tree/roots-data.glsl"
#pragma include "proc-tree/roots-common.glsl"

#ifndef IS_SHADOW
layout (location = 0) out VS_OUT vs_out;
#endif

vec3 instance_scale(float radius) {
#ifdef IS_SHADOW
  radius = max(radius, 0.25);
#endif
  return vec3(radius, 1.0, radius);
}

void main() {
  vec3 instance_position = instance_position_radius.xyz;
  vec3 child_instance_position = child_instance_position_radius.xyz;
  float self_radius = instance_position_radius.w;
  float child_radius = child_instance_position_radius.w;

  vec3 s = shape_function(num_points_xz_sun_position_xy.xy);
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
  vec3 p = mix(p_base, p_tip, y);

#ifndef IS_SHADOW
  vs_out.normal = n;
#endif

  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}