#version 450
#extension GL_ARB_separate_shader_objects : enable

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#define INSTANCE_UNIFORM_SET (1)
#define INSTANCE_UNIFORM_BINDING (0)

#pragma include "proc-tree/flower-leaves-data-common.glsl"
#pragma include "proc-tree/flower-leaves-data.glsl"

layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;
layout (std140, set = INSTANCE_UNIFORM_SET, binding = 1) readonly buffer TransformBuffer {
  PetalTransformData transform_data[];
};

layout (location = 0) in vec4 position_petal_randomness_transform_index;
layout (location = 1) in vec4 translation_petal_fraction;
layout (location = 2) in vec4 instance_position_transform_buffer_index;
layout (location = 3) in vec2 instance_direction;
layout (location = 4) in uvec4 packed_axis_root_info0;
layout (location = 5) in uvec4 packed_axis_root_info1;
layout (location = 6) in uvec4 packed_axis_root_info2;

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "z_rotation.glsl"
#pragma include "rotation2.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/wind-displacement.glsl"

#ifdef USE_HEMISPHERE_COLOR_IMAGE

#pragma include "spherical-to-uv.glsl"

#endif

vec3 aabb_p0() {
  return aabb_p0_padded.xyz;
}

vec3 aabb_p1() {
  return aabb_p1_padded.xyz;
}

vec2 x_limits() {
  return vec2(-petal_params.scale_x, petal_params.scale_x);
}

vec2 z_limits() {
  return vec2(0.0, petal_params.scale_y);
}

vec3 sin_shape_function(vec2 scale_xz) {
  float z = position_petal_randomness_transform_index.y / num_points.y;
  float x = position_petal_randomness_transform_index.x / floor(num_points.x * 0.5);
  float z_incr = 1.0 / num_points.y;
  float z_out = (1.0 - z) - z_incr;
  float use_x = sin(PI * z) * x * scale_xz.x;
  float use_z = z_out * scale_xz.y;
  return vec3(use_x, 0.0, use_z);
}

float radius_function(float z01, in PetalParameters params) {
  float r = params.radius + translation_petal_fraction.w * params.max_additional_radius;
  r = pow(z01, params.radius_power) * r;
  r = max(params.min_radius, r);
  return r;
}

float circumference_function(float z01, in PetalParameters params) {
  return mix(params.circum_frac0, params.circum_frac1, pow(z01, params.circum_frac_power));
}

vec3 transform_petal_point(vec3 p, in PetalParameters params, out vec2 relative_position) {
  vec2 min_max_xs = x_limits();
  float x11 = (p.x - min_max_xs.x) / (min_max_xs.y - min_max_xs.x) * 2.0 - 1.0;

  vec2 min_max_zs = z_limits();
  float z01 = (p.z - min_max_zs.x) / (min_max_zs.y - min_max_zs.x);

  relative_position = vec2(x11, z01);

  float radius = radius_function(z01, params);
  float circum_frac = circumference_function(z01, params);
  float base_x = radius * p.x;
  float base_y = radius;

  float theta = p.x * PI * circum_frac;
  p = vec3(sin(theta) * radius, -cos(theta) * radius + p.y, p.z);

  float petal_frac = translation_petal_fraction.w;
  float z_rot = 2.0 * PI * petal_frac;
  p = z_rotation(z_rot) * p;
  p = vec3(p.x, p.z, p.y);
  //  Lower based on petal fraction.
  p.y -= petal_params.max_negative_y_offset * petal_frac;
  return p;
}

vec2 sample_wind() {
  vec3 world_center = (aabb_p1() - aabb_p0()) * 0.5 + aabb_p0();
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

vec2 alt_wind_oscillation(vec2 sampled_wind, float elapsed_time, float rand01, float wind_fast_osc_amplitude) {
  float wind_len = length(sampled_wind);
  float d0 = wind_displacement_limits_wind_strength_limits.x;
  float d1 = wind_displacement_limits_wind_strength_limits.y;
  float tval = smoothstep(d0, d1, wind_len);
  float a = mix(0.25, 1.0, tval);

  float base_freq = 12.0;
#if 0
  vec2 base = sampled_wind * cos(elapsed_time * (base_freq + (rand01 * 2.0 - 1.0) * 4.0)) * a;
  vec2 addtl = sampled_wind * cos(elapsed_time * base_freq * 2.0 + 2.0 * PI * rand01) * wind_fast_osc_amplitude;
#else
  vec2 scaled_cam_xz = wind_len * camera_right_xz.xy;
  vec2 base = scaled_cam_xz * cos(elapsed_time * (base_freq + (rand01 * 2.0 - 1.0) * 4.0)) * a;
  vec2 addtl = scaled_cam_xz * cos(elapsed_time * base_freq * 2.0 + 2.0 * PI * rand01) * wind_fast_osc_amplitude;
#endif
  return base + addtl;
}

void main() {
  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(packed_axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(packed_axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(packed_axis_root_info2, root_info2, child_root_info2);

  vec2 petal_params_scale = vec2(petal_params.scale_x, petal_params.scale_y);
  float t = time_info.x;
  float flower_rand1 = translation_petal_fraction.z;
  float petal_rand = position_petal_randomness_transform_index.z;
  float petal_rand11 = petal_rand * 2.0 - 1.0;

  vec2 sampled_wind = sample_wind();
  float wind_atten = wind_attenuation(
    sampled_wind, wind_displacement_limits_wind_strength_limits.xy, wind_displacement_limits_wind_strength_limits.zw);

  vec3 shape = sin_shape_function(petal_params_scale);
  vec2 relative_position = vec2(0.0);
  shape = transform_petal_point(shape, petal_params, relative_position);
  shape = x_rotation(PI * 0.5) * shape;
  shape = y_rotation(2.0 * PI * flower_rand1) * shape;
  shape.y += translation_petal_fraction.y;

  vec3 use_model_scale = model_scale_shadow_scale.xyz;

  mat3 m = make_coordinate_system_y(spherical_to_cartesian(instance_direction));
  vec3 pos = m * (shape * use_model_scale) + instance_position_transform_buffer_index.xyz;
  vec3 pos_wind = wind_displacement(pos, t, aabb_p0(), aabb_p1(), root_info0, root_info1, root_info2, wind_atten);

  float wind_fast_osc_amplitude = petal_params.wind_fast_osc_amplitude;
  vec2 wind_displace = alt_wind_oscillation(sampled_wind, t, petal_rand, wind_fast_osc_amplitude) * pow(relative_position.y, 1.0) * 0.5;
  pos.xz += wind_displace;
  pos_wind.xz += wind_displace;

  float shadow_scale = model_scale_shadow_scale.w;
  vec3 shadow_pos = m * (shape * use_model_scale * shadow_scale) + instance_position_transform_buffer_index.xyz;
  //  petal transform
  int base_transform_buffer_index = int(instance_position_transform_buffer_index.w);
  int petal_transform_buffer_index = int(position_petal_randomness_transform_index.w);
  int transform_buffer_index = base_transform_buffer_index + petal_transform_buffer_index;
  vec3 displace_translation = transform_data[transform_buffer_index].translation.xyz;
  shadow_pos += displace_translation;
  pos_wind += displace_translation;

  vs_out.v_uv = vec2(0.0, 0.0);
  vs_out.v_light_space_position0 = vec3(sun_light_view_projection0 * vec4(shadow_pos, 1.0));
  vs_out.v_shadow_position = shadow_pos;
  vs_out.v_relative_position = relative_position;
  vs_out.v_animation_t = 0.0;
  vs_out.v_petal_rand = petal_rand;

#ifdef USE_HEMISPHERE_COLOR_IMAGE
  vec3 hemi_center = (aabb_p1() - aabb_p0()) * 0.5 + aabb_p0();
  hemi_center.y = aabb_p0().y;
//  vec3 hemi_n = normalize(instance_position_transform_buffer_index.xyz - hemi_center);
  vec3 hemi_n = normalize(pos_wind - hemi_center);
  vs_out.v_hemisphere_uv = spherical_to_uv(cartesian_to_spherical(hemi_n));
//  vs_out.v_hemisphere_uv += mod(wind_fast_osc_scale_unused.y * 0.01 + wind_fast_osc_scale_unused.z, 1.0);
#endif

  gl_Position = projection * view * vec4(pos_wind, 1.0);
}