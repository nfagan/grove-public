#version 450

struct Instance {
  vec4 translation_curl_scale;
  uvec4 forwards_instance_randomness_uv_offset;
  vec4 right_global_scale;
  vec4 aabb_p0;
  vec4 aabb_p1;
  uvec4 axis_root_info0;
  uvec4 axis_root_info1;
  uvec4 axis_root_info2;
};

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

layout (std140, set = 0, binding = 0) readonly buffer InstanceBuffer {
  Instance instances[];
};

#ifndef IS_SHADOW
layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;
layout (set = 0, binding = 2, std140) uniform UniformBuffer {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 shadow_proj_view;
  vec4 camera_position_alpha_test_enabled;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
  vec4 sun_position;
  vec4 sun_color;
};
#endif

#ifndef IS_SHADOW
layout (location = 0) out vec2 v_uv;
layout (location = 1) out vec2 v_hemisphere_uv;
layout (location = 2) out vec3 v_shadow_position;
layout (location = 3) out vec3 v_normal;
#endif

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 data0;
  vec4 cam_axis0;
  vec4 cam_axis1;
  vec4 cam_axis2;
};

mat3 parse_view() {
  //  @NOTE: swap
  return mat3(cam_axis0.xyz, cam_axis2.xyz, cam_axis1.xyz);
}

#ifdef IS_SHADOW
float parse_shadow_scale() {
  return cam_axis0.w;
}
#endif

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "spherical-to-uv.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "z_rotation.glsl"

#ifndef IS_SHADOW

#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/wind-displacement.glsl"

vec3 wind_displacement(in Instance inst, vec3 inst_trans, float t) {
  vec2 world_center_xz = ((inst.aabb_p1.xyz - inst.aabb_p0.xyz) * 0.5 + inst.aabb_p0.xyz).xz;
  vec2 sampled_wind = sample_wind_tip_displacement(world_center_xz, wind_world_bound_xz, wind_displacement_texture);
  float wind_atten = wind_attenuation(
    sampled_wind, wind_displacement_limits_wind_strength_limits.xy, wind_displacement_limits_wind_strength_limits.zw);

  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(inst.axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(inst.axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(inst.axis_root_info2, root_info2, child_root_info2);

  vec3 displaced = wind_displacement(
    inst_trans, t, inst.aabb_p0.xyz, inst.aabb_p1.xyz, root_info0, root_info1, root_info2, wind_atten);
  return displaced - inst_trans;
}

#endif

void main() {
  Instance inst = instances[instance_index];

  float t = data0.x;

  vec3 forwards = uintBitsToFloat(inst.forwards_instance_randomness_uv_offset.xyz);
  float inst_rand = float(inst.forwards_instance_randomness_uv_offset.w & 0xffffu) / float(0xffffu);
  float uv_off = float((inst.forwards_instance_randomness_uv_offset.w >> 16u) & 0xffffu) / float(0xffffu);

  vec3 translation = inst.translation_curl_scale.xyz;
  float curl_scale = inst.translation_curl_scale.w;
  float global_scale = inst.right_global_scale.w;

  vec3 p = vec3(position, 0.0);
  //  scale
#ifndef IS_FIXED_SHADOW
  p *= global_scale * (1.0 + (inst_rand * 2.0 - 1.0) * 0.125);
#endif
  //  curl
#ifdef IS_SHADOW
  p *= parse_shadow_scale();
#endif
#ifndef IS_FIXED_SHADOW
  p.z += pow(abs(position.x), 2.0) * curl_scale * global_scale;
#endif

  vec3 shadow_p = p;

#ifndef IS_SHADOW
  float osc_scale = 0.125 * pow((position.y * 0.5 + 0.5), 2.0);
  float rate = 1.5 + (inst_rand * 2.0 - 1.0) * 0.25;
  p.z += cos(PI * t * rate + 4.0 * (position.x * 0.5 + 0.5) + inst_rand * PI) * osc_scale;
#endif

  mat3 yrot = y_rotation(inst.aabb_p0.w);
  p = yrot * p;
  shadow_p = yrot * shadow_p;

#if 1
  mat3 zrot = z_rotation(inst.aabb_p1.w);
  vec3 vrot = zrot * vec3(0.0, 1.5, 0.0);
  p = zrot * p + vrot;
  shadow_p = zrot * shadow_p + vrot;

  p = vec3(p.x, p.z, p.y);
  shadow_p = vec3(shadow_p.x, shadow_p.z, shadow_p.y);

  vec3 right = inst.right_global_scale.xyz;
  vec3 up = normalize(cross(forwards, right));
  right = cross(up, forwards);
  mat3 m = mat3(right, up, forwards);

  #if 0
  p = m * p;
  shadow_p = m * shadow_p;

  mat3 inv_view = parse_view();
  vec3 p_inv = inv_view * p;
  vec3 shadow_p_inv = inv_view * shadow_p;

  const float min_dist = 210.0;
  const float max_dist = 220.0;
  float dist = clamp(length(data0.yzw - translation), min_dist, max_dist);
  dist = (dist - min_dist) / (max_dist - min_dist);

  p = mix(p, p_inv, dist) + translation;
  shadow_p = mix(shadow_p, shadow_p_inv, dist) + translation;

  #else
  p = m * p + translation;
  shadow_p = m * shadow_p + translation;
  #endif
#else
  mat3 zrot = z_rotation(inst.aabb_p1.w);
  vec3 vrot = zrot * vec3(0.0, 1.5, 0.0);
  p = zrot * p + vrot;
  shadow_p = zrot * shadow_p + vrot;

  p = vec3(p.x, p.z, p.y);
  shadow_p = vec3(shadow_p.x, shadow_p.z, shadow_p.y);

  mat3 m = mat3(inst.right_global_scale.xyz, cross(forwards, inst.right_global_scale.xyz), forwards);
  p = m * p + translation;
  shadow_p = m * shadow_p + translation;
#endif

#ifndef IS_SHADOW
  p += wind_displacement(inst, translation, t);
#endif

#ifndef IS_SHADOW
  vec3 world_center = (inst.aabb_p1.xyz - inst.aabb_p0.xyz) * 0.5 + inst.aabb_p0.xyz;
  vec3 hemi_n = normalize(translation - vec3(world_center.x, inst.aabb_p0.y, world_center.z));
  v_hemisphere_uv = spherical_to_uv(cartesian_to_spherical(hemi_n)) + uv_off;
  v_uv = position * 0.5 + 0.5;
  v_shadow_position = shadow_p;
  v_normal = normalize(translation - world_center);
#endif

  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}
