#version 450
#extension GL_ARB_separate_shader_objects : enable

#define USE_BLOWING_LEAVES (0)

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

vec3 axis_root_position_to_world(vec3 root_p) {
  vec3 span = aabb_p1() - aabb_p0();
  return span * root_p + aabb_p0();
}

vec3 axis_rotation(vec3 p, vec3 root, float theta) {
  vec3 p_off = p - root;
  mat3 m = x_rotation(theta);
  p_off = m * p_off;
  return p_off + root;
}

vec2 xz_origin_relative_position(vec3 p) {
  vec2 xz_span = (aabb_p1() - aabb_p0()).xz;
  vec2 xz_min = aabb_p0().xz;
  vec2 xz01 = (p.xz - xz_min) / xz_span;
  return xz01 * 2.0 - 1.0;
}

float y_position01(vec3 p) {
  return (p.y - aabb_p0().y) / (aabb_p1().y - aabb_p0().y);
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
  vec3 world_center = (aabb_p1() - aabb_p0()) * 0.5 + aabb_p0();
  vec2 world_xz = world_center.xz;
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

float wind_attenuation(vec2 sampled_wind) {
  float d0 = wind_displacement_limits_wind_strength_limits.x;
  float d1 = wind_displacement_limits_wind_strength_limits.y;
  float a = smoothstep(d0, d1, length(sampled_wind));
  return mix(wind_displacement_limits_wind_strength_limits.z, wind_displacement_limits_wind_strength_limits.w, a);
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

#if USE_BLOWING_LEAVES
vec3 blowing_leaves_speed(float t, float anim_t, float petal_rand, float petal_rand11) {
  float init_speed_xz = (6.0 + petal_rand11 * 8.0 * pow(1.0 - anim_t, 4.0 + petal_rand11 * 0.25));
  float speed_xz_osc = 2.0 * petal_rand * (cos(t * 2.0 * petal_rand) * 0.5 + 0.5);
  float speed_xz = init_speed_xz + 8.0 + 4.0 * petal_rand11 + speed_xz_osc;
  float speed_y = 0.5 + petal_rand11 * 0.25 + petal_rand11 * 0.1 * (cos(t * (1.0 + 2.0 * petal_rand)) * 0.5 + 0.5);
  return vec3(speed_xz, speed_y, speed_xz);
}

vec3 blowing_leaves_direction(vec2 sampled_wind, float petal_rand, float petal_rand11) {
  vec2 move_vxz = normalize(sampled_wind);
  float wind_rot = PI * 0.1 * petal_rand11;
  move_vxz = rotation2(wind_rot) * move_vxz;
  return vec3(move_vxz.x, -(3.0 + petal_rand * 4.0), move_vxz.y);
}
#endif

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
  float wind_atten = wind_attenuation(sampled_wind);

  vec3 shape = sin_shape_function(petal_params_scale);
  vec2 relative_position = vec2(0.0);
  shape = transform_petal_point(shape, petal_params, relative_position);
  shape = x_rotation(PI * 0.5) * shape;
  shape = y_rotation(2.0 * PI * flower_rand1) * shape;
  shape.y += translation_petal_fraction.y;

  //  blowing leaves animation
#if USE_BLOWING_LEAVES
  float blowing_petal_t = blowing_petal_params.time_info.x;
  float anim_dur = blowing_petal_params.time_info.y;
  float mod_t = clamp(blowing_petal_params.t, 0.0, anim_dur);
  float anim_t = mod_t / anim_dur;

  vec3 blowing_scale = blowing_petal_params.scale.xyz;
  vec3 use_model_scale = mix(
    model_scale_shadow_scale.xyz, blowing_scale, pow(min(1.0, anim_t * (8.0 + petal_rand11 * 4.0)), 1.0));
#else
  vec3 use_model_scale = model_scale_shadow_scale.xyz;
  float anim_t = 0.0;
#endif

  mat3 m = make_coordinate_system_y(spherical_to_cartesian(instance_direction));
  vec3 pos = m * (shape * use_model_scale) + instance_position_transform_buffer_index.xyz;
  vec3 pos_wind = wind_displacement(pos, t, root_info0, root_info1, root_info2, wind_atten);

  float wind_fast_osc_amplitude = petal_params.wind_fast_osc_amplitude;
  vec2 wind_displace = alt_wind_oscillation(sampled_wind, t, petal_rand, wind_fast_osc_amplitude) * pow(relative_position.y, 1.0) * 0.5;
  pos.xz += wind_displace;
  pos_wind.xz += wind_displace;

  //  blowing leaves animation
#if USE_BLOWING_LEAVES
  vec3 blowing_anim_pos = mix(pos_wind, pos, min(1.0, anim_t * 32.0));
  vec3 blowing_move_v = blowing_leaves_direction(sampled_wind, petal_rand, petal_rand11);
  vec3 blowing_speed = blowing_leaves_speed(t, anim_t, petal_rand, petal_rand11);
  vec3 blowing_vel = blowing_move_v * blowing_speed;
  blowing_anim_pos += blowing_vel * mod_t;
  pos_wind = blowing_anim_pos;
#endif

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
  vs_out.v_animation_t = anim_t;
  vs_out.v_petal_rand = petal_rand;

  gl_Position = projection * view * vec4(pos_wind, 1.0);
}