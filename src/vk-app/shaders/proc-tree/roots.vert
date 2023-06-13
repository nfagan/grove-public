#version 450

//  vertices
layout (location = 0) in vec2 position;
//  dynamic
layout (location = 1) in vec4 instance_directions;
layout (location = 2) in vec4 instance_position_radius;
layout (location = 3) in vec4 child_instance_position_radius;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "proc-tree/common.glsl"
#pragma include "proc-tree/roots-data.glsl"

layout (location = 0) out VS_OUT vs_out;

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

void main() {
  vec3 instance_position = instance_position_radius.xyz;
  vec3 child_instance_position = child_instance_position_radius.xyz;
  float self_radius = instance_position_radius.w;
  float child_radius = child_instance_position_radius.w;

  vec2 instance_direction = instance_directions.xy;
  vec2 child_instance_direction = instance_directions.zw;

  vec3 d_base = spherical_to_cartesian(instance_direction);
  vec3 d_tip = spherical_to_cartesian(child_instance_direction);

  vec3 s = shape_function(num_points_xz_sun_position_xy.xy);
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  vec3 s_base = s0 * instance_scale(self_radius);
  vec3 s_tip = s0 * instance_scale(child_radius);

  mat3 base_coord_sys = make_coordinate_system_y(d_base);
  mat3 tip_coord_sys = make_coordinate_system_y(d_tip);

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

  vs_out.normal = n;

  gl_Position = projection_view * vec4(p, 1.0);
}