#version 450

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)
#pragma include "proc-tree/branch-shadow-data.glsl"

//  vertices
layout (location = 0) in vec2 position;
//  static
layout (location = 1) in vec4 instance_directions;
layout (location = 2) in uvec4 packed_axis_root_info0;
layout (location = 3) in uvec4 packed_axis_root_info1;
layout (location = 4) in uvec4 packed_axis_root_info2;
layout (location = 5) in vec4 aabb_p0_instance_index;
layout (location = 6) in vec3 aabb_p1;
//  dynamic
layout (location = 7) in vec4 instance_position_radius;
layout (location = 8) in vec4 child_instance_position_radius;

#pragma include "frame.glsl"
#pragma include "pi.glsl"
#pragma include "proc-tree/common.glsl"

vec3 shadow_instance_scale(float self_radius, float radius, float min_radius_shadow, float max_radius_scale_shadow) {
  vec3 aabb_p0 = aabb_p0_instance_index.xyz;
  float frac_y = (instance_position_radius.y - aabb_p0.y) / (aabb_p1.y - aabb_p0.y);
  float scale = self_radius < min_radius_shadow ? 0.0 : mix(1.0, max_radius_scale_shadow, pow(frac_y, 0.5));
  return vec3(radius * scale, 1.0, radius * scale);
}

void main() {
  vec2 instance_direction = instance_directions.xy;
  vec2 child_instance_direction = instance_directions.zw;
  vec2 num_points_xz = num_points_xz_radius_info.xy;
  float min_radius_shadow = num_points_xz_radius_info.z;
  float max_radius_scale_shadow = num_points_xz_radius_info.w;
  vec3 instance_position = instance_position_radius.xyz;
  vec3 child_instance_position = child_instance_position_radius.xyz;
  float self_radius = instance_position_radius.w;
  float child_radius = child_instance_position_radius.w;

  vec3 d_base = spherical_to_cartesian(instance_direction);
  vec3 d_tip = spherical_to_cartesian(child_instance_direction);

  vec3 s = shape_function(num_points_xz);
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  vec3 s_base = s0 * shadow_instance_scale(self_radius, self_radius, min_radius_shadow, max_radius_scale_shadow);
  vec3 s_tip = s0 * shadow_instance_scale(self_radius, child_radius, min_radius_shadow, max_radius_scale_shadow);

  mat3 base_coord_sys = make_coordinate_system_y(d_base);
  mat3 tip_coord_sys = make_coordinate_system_y(d_tip);

  vec3 p_base = base_coord_sys * s_base + instance_position;
  vec3 p_tip = tip_coord_sys * s_tip + child_instance_position;
  vec3 p = mix(p_base, p_tip, y);

  vec4 trans_pos = light_view_projection * vec4(p, 1.0);
  trans_pos.z = (trans_pos.z * 0.5 + 0.5);
  gl_Position = trans_pos;
}