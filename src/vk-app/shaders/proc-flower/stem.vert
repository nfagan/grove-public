#version 450

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#pragma include "proc-flower/stem-data.glsl"

layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;

//  vertices
layout (location = 0) in vec2 position;
//  static
layout (location = 1) in vec4 instance_directions;
layout (location = 2) in vec4 aabb_p0;
layout (location = 3) in vec4 aabb_p1;
//  dynamic
layout (location = 4) in vec3 instance_position;
layout (location = 5) in vec3 child_instance_position;
layout (location = 6) in vec2 instance_radii;  //  x = instance radius, y = child instance radius.

layout (location = 0) out vec3 v_world_position;
layout (location = 1) out vec3 v_light_space_position0;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "grid-geometry/cylinder.glsl"
#pragma include "x_rotation.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

vec3 shape_function() {
  return cylinder_shape_function(num_points_xz_t.xy, position, PI);
}

vec2 world_origin_xz() {
  vec3 world_center = (aabb_p1.xyz - aabb_p0.xyz) * 0.5 + aabb_p0.xyz;
  return world_center.xz;
}

vec2 wind_displacement(vec3 p, float y_frac, vec2 sampled_wind, float t) {
  float phase = length(world_origin_xz());
  return sampled_wind * tip_wind_displacement_fraction(y_frac) * sin(t * 5.0 + phase) * 0.1;
}

vec2 sample_wind() {
  return sample_wind_tip_displacement(world_origin_xz(), wind_world_bound_xz, wind_displacement_texture);
}

void main() {
  vec2 sampled_wind = sample_wind();
  float t = num_points_xz_t.z;
  float wind_scale = color_wind_influence_enabled.w;

  vec2 instance_direction = instance_directions.xy;
  vec2 child_instance_direction = instance_directions.zw;

  vec3 d_base = spherical_to_cartesian(instance_direction);
  vec3 d_tip = spherical_to_cartesian(child_instance_direction);

  vec3 s = shape_function();
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  vec3 s_base = s0 * instance_scale(instance_radii.x);
  vec3 s_tip = s0 * instance_scale(instance_radii.y);

  mat3 base_coord_sys = make_coordinate_system_y(d_base);
  mat3 tip_coord_sys = make_coordinate_system_y(d_tip);

  vec3 p_base = base_coord_sys * s_base + instance_position;
  vec3 p_tip = tip_coord_sys * s_tip + child_instance_position;

  float y_frac_base = aabb_p0.w;
  float y_frac_tip = aabb_p1.w;

  p_base.xz += wind_displacement(instance_position, y_frac_base, sampled_wind, t) * wind_scale;
  p_tip.xz += wind_displacement(child_instance_position, y_frac_tip, sampled_wind, t) * wind_scale;
  vec3 p = mix(p_base, p_tip, y);

  v_world_position = p;
  v_light_space_position0 = (sun_light_view_projection0 * vec4(p, 1.0)).xyz;

  gl_Position = projection * view * vec4(p, 1.0);
}
