#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

struct RenderBranchNodeDynamicData {
  vec4 self_p_self_r;
  vec4 child_p_child_r;
};

struct RenderBranchNodeStaticData {
  uvec4 directions0;
  uvec4 directions1;
  uvec4 aggregate_index_unused;
};

struct RenderWindBranchNodeStaticData {
  uvec4 directions0;
  uvec4 directions1;
  uvec4 aggregate_index_unused;
  uvec4 wind_info0;
  uvec4 wind_info1;
  uvec4 wind_info2;
};

struct RenderBranchNodeAggregate {
  vec4 aabb_p0_unused;
  vec4 aabb_p1_unused;
};

struct VS_OUT {
  vec3 normal;
  vec3 light_space_position0;
  vec3 shadow_position;
};

layout (set = 0, binding = 0, std140) uniform UniformData {
  vec4 num_points_xz_t;
  vec4 wind_displacement_info;  //  vec2 wind_displacement_limits, vec2 wind_strength_limits
  vec4 wind_world_bound_xz;
//  Shadow info.
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 shadow_info; //  min_radius_shadow, max_radius_scale_shadow, unused, unused
//  Frag info.
  vec4 sun_position;
  vec4 sun_color;
  vec4 camera_position;
  vec4 color;
#pragma include "shadow/sample-struct-fields.glsl"
};

layout (set = 0, binding = 1, std430) readonly buffer DynamicInstances {
  RenderBranchNodeDynamicData dynamic_instances[];
};

layout (set = 0, binding = 2, std430) readonly buffer StaticInstances {
#ifdef IS_WIND
  RenderWindBranchNodeStaticData static_instances[];
#else
  RenderBranchNodeStaticData static_instances[];
#endif
};

layout (set = 0, binding = 3, std430) readonly buffer AggregateData {
  RenderBranchNodeAggregate aggregates[];
};

#ifndef IS_SHADOW
#ifdef IS_WIND
layout (set = 0, binding = 4) uniform sampler2D wind_displacement_texture;
#endif
#endif

layout (push_constant, std140) uniform PushConstantData {
  mat4 projection_view;
};

#ifndef IS_SHADOW
layout (location = 0) out VS_OUT vs_out;
#endif

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/common.glsl"
#pragma include "proc-tree/wind-displacement.glsl"

void unpack_self_child(uvec4 a, out vec4 sf, out vec4 cf) {
  const uvec4 mask = uvec4(0xffff);
  const vec4 maskf = vec4(0xffff);

  uvec4 child = (a >> 16u) & mask;
  uvec4 self = a & mask;

  sf = vec4(self) / maskf * 2.0 - 1.0;
  cf = vec4(child) / maskf * 2.0 - 1.0;
}

mat3 unpack_matrix(vec4 unpack0, vec4 unpack1) {
  vec3 x = vec3(unpack0.x, unpack0.y, unpack0.z);
  vec3 y = vec3(unpack0.w, unpack1.x, unpack1.y);
  vec3 z = cross(y, x);
  return mat3(x, y, z);
}

void unpack_coord_sys(uvec4 directions0, uvec4 directions1, out mat3 self, out mat3 child) {
  vec4 unpack_self0;
  vec4 unpack_child0;
  unpack_self_child(directions0, unpack_self0, unpack_child0);

  vec4 unpack_self1;
  vec4 unpack_child1;
  unpack_self_child(directions1, unpack_self1, unpack_child1);

  self = unpack_matrix(unpack_self0, unpack_self1);
  child = unpack_matrix(unpack_child0, unpack_child1);
}

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

float y_aabb_fraction(float inst_y, vec3 aabb_p0, vec3 aabb_p1) {
  return (inst_y - aabb_p0.y) / (aabb_p1.y - aabb_p0.y);
}

vec3 shadow_instance_scale(float frac_y, float self_radius, float radius,
                           float min_radius_shadow, float max_radius_scale_shadow) {
  float scale = self_radius < min_radius_shadow ? 0.0 : mix(1.0, max_radius_scale_shadow, pow(frac_y, 0.5));
  return vec3(radius * scale, 1.0, radius * scale);
}

#ifndef IS_SHADOW
#ifdef IS_WIND
vec2 sample_wind(vec3 aabb_p0, vec3 aabb_p1, vec4 wind_world_bound_xz) {
  vec3 world_center = (aabb_p1 - aabb_p0) * 0.5 + aabb_p0;
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}
#endif
#endif

void main() {
  RenderBranchNodeDynamicData dyn_inst = dynamic_instances[instance_index];
#ifdef IS_WIND
  RenderWindBranchNodeStaticData static_inst = static_instances[instance_index];
#else
  RenderBranchNodeStaticData static_inst = static_instances[instance_index];
#endif
  RenderBranchNodeAggregate aggregate = aggregates[static_inst.aggregate_index_unused.x];
  vec3 aabb_p0 = aggregate.aabb_p0_unused.xyz;
  vec3 aabb_p1 = aggregate.aabb_p1_unused.xyz;

  mat3 base_coord_sys;
  mat3 tip_coord_sys;
  unpack_coord_sys(static_inst.directions0, static_inst.directions1, base_coord_sys, tip_coord_sys);

  float self_radius = dyn_inst.self_p_self_r.w;
  vec3 self_p = dyn_inst.self_p_self_r.xyz;
  float child_radius = dyn_inst.child_p_child_r.w;
  vec3 child_p = dyn_inst.child_p_child_r.xyz;

  vec3 s = shape_function(num_points_xz_t.xy);
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

  //  Shadow position - no wind influence.
  float min_radius_shadow = shadow_info.x;
  float max_radius_scale_shadow = shadow_info.y;
  float inst_y = y_aabb_fraction(self_p.y, aabb_p0, aabb_p1);
  vec3 s_base_shadow = s0 * shadow_instance_scale(inst_y, self_radius, self_radius, min_radius_shadow, max_radius_scale_shadow);
  vec3 s_tip_shadow = s0 * shadow_instance_scale(inst_y, self_radius, child_radius, min_radius_shadow, max_radius_scale_shadow);
  vec3 p_shadow = mix(base_coord_sys * s_base_shadow + self_p, tip_coord_sys * s_tip_shadow + child_p, y);

#ifndef IS_SHADOW
  vec3 s_base = s0 * instance_scale(self_radius);
  vec3 s_tip = s0 * instance_scale(child_radius);

  vec3 s0n = normalize(s0);
  vec3 n_base = normalize(base_coord_sys * s0n);
  vec3 n_tip = normalize(tip_coord_sys * s0n);
  vec3 n = mix(n_base, n_tip, y);

  if (length(n) == 0.0) {
    n = vec3(0.0, 1.0, 0.0);
  }

  vec3 p_base = base_coord_sys * s_base + self_p;
  vec3 p_tip = tip_coord_sys * s_tip + child_p;

#ifdef IS_WIND
  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(static_inst.wind_info0, root_info0, child_root_info0);
  unpack_axis_root_info(static_inst.wind_info1, root_info1, child_root_info1);
  unpack_axis_root_info(static_inst.wind_info2, root_info2, child_root_info2);

  vec2 wind_displacement_limits = wind_displacement_info.xy;
  vec2 wind_strength_limits = wind_displacement_info.zw;
  float t = num_points_xz_t.z;

  vec2 sampled_wind = sample_wind(aabb_p0, aabb_p1, wind_world_bound_xz);
  float wind_atten = wind_attenuation(sampled_wind, wind_displacement_limits, wind_strength_limits);

  p_base = wind_displacement(p_base, t, aabb_p0, aabb_p1, root_info0, root_info1, root_info2, wind_atten);
  p_tip = wind_displacement(p_tip, t, aabb_p0, aabb_p1, child_root_info0, child_root_info1, child_root_info2, wind_atten);
#endif

  vec3 p = mix(p_base, p_tip, y);

  vs_out.normal = n;
  vs_out.light_space_position0 = vec3(sun_light_view_projection0 * vec4(p_shadow, 1.0));
  vs_out.shadow_position = p_shadow;

#else //  ifndef IS_SHADOW
  vec3 p = p_shadow;
#endif

  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}
