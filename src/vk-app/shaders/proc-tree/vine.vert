#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 self_translation_radius;
layout (location = 2) in vec4 child_translation_radius;
layout (location = 3) in uvec4 directions0;
layout (location = 4) in uvec4 directions1;
layout (location = 5) in uvec4 self_aggregate_index_child_aggregate_index_unused;
layout (location = 6) in uvec4 wind_info0;
layout (location = 7) in uvec4 wind_info1;
layout (location = 8) in uvec4 wind_info2;

layout (location = 0) out vec3 v_color;

struct AggregateRenderData {
  vec4 wind_aabb_p0;
  vec4 wind_aabb_p1;
};

layout (set = 0, binding = 0) readonly buffer AttachedToAggregateRenderData {
  AggregateRenderData aggregates[];
};

layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;

#pragma include "pi.glsl"
#pragma include "x_rotation.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/roots-common.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/wind-displacement.glsl"

layout (push_constant, std140) uniform PushConstantData {
  mat4 projection_view;
  vec4 vine_color_t;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
};

vec2 sample_wind(vec3 wind_aabb_p0, vec3 wind_aabb_p1) {
  vec3 world_center = (wind_aabb_p1 - wind_aabb_p0) * 0.5 + wind_aabb_p0;
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

float compute_wind_attenuation(vec2 sampled_wind) {
  vec2 wind_displacement_limits = wind_displacement_limits_wind_strength_limits.xy;
  vec2 wind_strength_limits = wind_displacement_limits_wind_strength_limits.zw;
  return wind_attenuation(sampled_wind, wind_displacement_limits, wind_strength_limits);
}

void main() {
  float self_radius = self_translation_radius.w;
  float child_radius = child_translation_radius.w;
  uint self_aggregate_index = self_aggregate_index_child_aggregate_index_unused.x;
  uint child_aggregate_index = self_aggregate_index_child_aggregate_index_unused.y;
  float t = vine_color_t.w;

  AggregateRenderData self_aggregate_data = aggregates[self_aggregate_index];
  AggregateRenderData child_aggregate_data = aggregates[child_aggregate_index];

  mat3 base_coord_sys;
  mat3 tip_coord_sys;
  unpack_coord_sys(base_coord_sys, tip_coord_sys);

//  vec3 p3 = vec3(position.y, position.x - 0.5, 0.0) * radius;
  vec3 p3 = vec3(position.x, 0.0, position.y);
  vec3 self_p = base_coord_sys * (p3 * self_radius) + self_translation_radius.xyz;
  vec3 child_p = tip_coord_sys * (p3 * child_radius) + child_translation_radius.xyz;

  vec4 self_root_info0;
  vec4 self_root_info1;
  vec4 self_root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(wind_info0, self_root_info0, child_root_info0);
  unpack_axis_root_info(wind_info1, self_root_info1, child_root_info1);
  unpack_axis_root_info(wind_info2, self_root_info2, child_root_info2);

  float self_wind_atten = compute_wind_attenuation(
    sample_wind(self_aggregate_data.wind_aabb_p0.xyz, self_aggregate_data.wind_aabb_p1.xyz));
  float child_wind_atten = compute_wind_attenuation(
    sample_wind(child_aggregate_data.wind_aabb_p0.xyz, child_aggregate_data.wind_aabb_p1.xyz));

  self_p = wind_displacement(self_p, t, self_aggregate_data.wind_aabb_p0.xyz, self_aggregate_data.wind_aabb_p1.xyz,
    self_root_info0, self_root_info1, self_root_info2, self_wind_atten);
  child_p = wind_displacement(child_p, t, child_aggregate_data.wind_aabb_p0.xyz, child_aggregate_data.wind_aabb_p1.xyz,
    child_root_info0, child_root_info1, child_root_info2, child_wind_atten);

  vec3 p = mix(self_p, child_p, position.z);

  v_color = vine_color_t.xyz;

  gl_Position = projection_view * vec4(p, 1.0);
}
