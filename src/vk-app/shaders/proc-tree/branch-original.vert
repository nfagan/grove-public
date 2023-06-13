#version 450

#define HAS_SWELL_FRAC (1)

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)
#pragma include "proc-tree/branch-data.glsl"

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

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/common.glsl"

layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;

layout (push_constant) uniform PushConstants {
  vec4 swell_info;
} push_constants;

int instance_index() {
  return int(aabb_p0_instance_index.w);
}

vec3 aabb_p0() {
  return aabb_p0_instance_index.xyz;
}

vec3 axis_root_position_to_world(vec3 root_p) {
  vec3 span = aabb_p1 - aabb_p0();
  return span * root_p + aabb_p0();
}

vec3 instance_scale(float radius) {
  return vec3(radius, 1.0, radius);
}

vec3 shadow_instance_scale(float inst_y, float self_radius, float radius, float min_radius_shadow, float max_radius_scale_shadow) {
  float frac_y = (inst_y - aabb_p0().y) / (aabb_p1.y - aabb_p0().y);
  float scale = self_radius < min_radius_shadow ? 0.0 : mix(1.0, max_radius_scale_shadow, pow(frac_y, 0.5));
  return vec3(radius * scale, 1.0, radius * scale);
}

vec3 axis_rotation(vec3 p, vec3 root, float theta) {
  vec3 p_off = p - root;
  mat3 m = x_rotation(theta);
  p_off = m * p_off;
  return p_off + root;
}

vec2 xz_origin_relative_position(vec3 p) {
  vec2 xz_span = (aabb_p1 - aabb_p0()).xz;
  vec2 xz_min = aabb_p0().xz;
  vec2 xz01 = (p.xz - xz_min) / xz_span;
  return xz01 * 2.0 - 1.0;
}

float y_position01(vec3 p) {
  return (p.y - aabb_p0().y) / (aabb_p1.y - aabb_p0().y);
}

vec3 wind_displacement(vec3 p, float t, vec4 root_info0, vec4 root_info1, vec4 root_info2, float wind_atten) {
  float theta_span = PI / 32.0;

  float y_frac = y_position01(p);
  vec2 xz_ori_relative = abs(xz_origin_relative_position(p));

  float xz_ori_rel = (xz_ori_relative.x + xz_ori_relative.y) * 0.5;
  float xz_atten = pow(xz_ori_rel, 4.0);
  float y_atten = pow(y_frac, 2.0);
  float atten = clamp(xz_atten + y_atten, 0.0, 1.0) * wind_atten;

  float is_active2 = root_info2.w;
  if (is_active2 >= 0.5) {
    vec3 root_p = root_info2.xyz;
    root_p = axis_root_position_to_world(root_p);
    p = axis_rotation(p, root_p, sin(t * 8.0 + length(root_p)) * theta_span * atten);
  }

  float is_active1 = root_info1.w;
  if (is_active1 >= 0.5) {
    vec3 root_p = root_info1.xyz;
    root_p = axis_root_position_to_world(root_p);
    p = axis_rotation(p, root_p, sin(t * 4.0 + length(root_p)) * theta_span * atten);
  }

  float is_active0 = root_info0.w;
  if (is_active0 >= 0.5) {
    vec3 root_p = root_info0.xyz;
    root_p = axis_root_position_to_world(root_p);
    p = axis_rotation(p, root_p, sin(t + length(root_p)) * theta_span * atten);
  }

  return p;
}

vec2 sample_wind() {
  vec3 world_center = (aabb_p1 - aabb_p0()) * 0.5 + aabb_p0();
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

float wind_attenuation(vec2 sampled_wind, vec2 wind_displacement_limits, vec2 wind_strength_limits) {
  float d0 = wind_displacement_limits.x;
  float d1 = wind_displacement_limits.y;
  float a = smoothstep(d0, d1, length(sampled_wind));
  return mix(wind_strength_limits.x, wind_strength_limits.y, a);
}

#if HAS_SWELL_FRAC
float compute_swell_frac(float y, vec3 instance_pos, vec3 child_instance_pos, vec2 swell_info) {
  const float crng = 0.1;
  const float strength_scale = 0.2;

  float yf = y_position01(mix(instance_pos, child_instance_pos, y));
  float cy = swell_info.x;
  float strength01 = swell_info.y;

  float cl = cy - crng;
  float cu = cy + crng;
  float rely = clamp(yf, cl, cu);
  float p01 = clamp(sin((rely - cl) / (cu - cl) * PI), 0.0, 1.0);

  return p01 * strength01 * strength_scale;
}
#endif

void main() {
  vec3 instance_position = instance_position_radius.xyz;
  vec3 child_instance_position = child_instance_position_radius.xyz;
  float self_radius = instance_position_radius.w;
  float child_radius = child_instance_position_radius.w;

  vec2 instance_direction = instance_directions.xy;
  vec2 child_instance_direction = instance_directions.zw;
  float t = num_points_xz_t.z;
  vec2 wind_displacement_limits = wind_displacement_info.xy;
  vec2 wind_strength_limits = wind_displacement_info.zw;
  float min_radius_shadow = shadow_info.x;
  float max_radius_scale_shadow = shadow_info.y;

  vec3 d_base = spherical_to_cartesian(instance_direction);
  vec3 d_tip = spherical_to_cartesian(child_instance_direction);

  vec3 s = shape_function(num_points_xz_t.xy);
  float y = s.y;
  vec3 s0 = vec3(s.x, 0.0, s.z);

#if HAS_SWELL_FRAC
  float swell_frac = compute_swell_frac(y, instance_position, child_instance_position, push_constants.swell_info.xy);
  float addtl_scl = mix(self_radius, child_radius, y) * 0.25 * swell_frac;
  vs_out.swell_frac = swell_frac;
#else
  const float addtl_scl = 0.0;
  vs_out.swell_frac = 0.0;
#endif

  vec3 s_base = s0 * instance_scale(self_radius + addtl_scl);
  vec3 s_tip = s0 * instance_scale(child_radius + addtl_scl);

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

  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(packed_axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(packed_axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(packed_axis_root_info2, root_info2, child_root_info2);

  vec2 sampled_wind = sample_wind();
  float wind_atten = wind_attenuation(sampled_wind, wind_displacement_limits, wind_strength_limits);

  p_base = wind_displacement(p_base, t, root_info0, root_info1, root_info2, wind_atten);
  p_tip = wind_displacement(p_tip, t, child_root_info0, child_root_info1, child_root_info2, wind_atten);
  vec3 p = mix(p_base, p_tip, y);

  //  Shadow position - no wind influence.
  float inst_y = instance_position_radius.y;
  vec3 s_base_shadow = s0 * shadow_instance_scale(inst_y, self_radius, self_radius, min_radius_shadow, max_radius_scale_shadow);
  vec3 s_tip_shadow = s0 * shadow_instance_scale(inst_y, self_radius, child_radius, min_radius_shadow, max_radius_scale_shadow);

  vec3 p_base_shadow = base_coord_sys * s_base_shadow + instance_position;
  vec3 p_tip_shadow = tip_coord_sys * s_tip_shadow + child_instance_position;
  vec3 p_shadow = mix(p_base_shadow, p_tip_shadow, y);

  vs_out.normal = n;
  vs_out.light_space_position0 = vec3(sun_light_view_projection0 * vec4(p_shadow, 1.0));
  vs_out.shadow_position = p_shadow;
  gl_Position = projection * view * vec4(p, 1.0);
}