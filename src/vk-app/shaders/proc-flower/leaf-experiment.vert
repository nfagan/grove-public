#version 450

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 translation_curl_scale;
layout (location = 2) in uvec4 forwards_instance_randomness_uv_offset;
layout (location = 3) in vec4 right_global_scale;
layout (location = 4) in vec4 aabb_p0;
layout (location = 5) in vec4 aabb_p1;
layout (location = 6) in uvec4 axis_root_info0;
layout (location = 7) in uvec4 axis_root_info1;
layout (location = 8) in uvec4 axis_root_info2;

#ifndef IS_SHADOW
layout (set = 0, binding = 0) uniform sampler2D wind_displacement_texture;
layout (set = 0, binding = 1, std140) uniform UniformBuffer {
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
};

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

vec3 wind_displacement(vec3 inst_trans, float t) {
  vec2 world_center_xz = ((aabb_p1.xyz - aabb_p0.xyz) * 0.5 + aabb_p0.xyz).xz;
  vec2 sampled_wind = sample_wind_tip_displacement(world_center_xz, wind_world_bound_xz, wind_displacement_texture);
  float wind_atten = wind_attenuation(
    sampled_wind, wind_displacement_limits_wind_strength_limits.xy, wind_displacement_limits_wind_strength_limits.zw);

  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(axis_root_info2, root_info2, child_root_info2);

  vec3 displaced = wind_displacement(
    inst_trans, t, aabb_p0.xyz, aabb_p1.xyz, root_info0, root_info1, root_info2, wind_atten);
  return displaced - inst_trans;
}

#endif

void main() {
  vec3 forwards = uintBitsToFloat(forwards_instance_randomness_uv_offset.xyz);
  float inst_rand = float(forwards_instance_randomness_uv_offset.w & 0xffffu) / float(0xffffu);
  float uv_off = float((forwards_instance_randomness_uv_offset.w >> 16u) & 0xffffu) / float(0xffffu);

  float t = data0.x;
  float curl_scale = translation_curl_scale.w;
  float global_scale = right_global_scale.w;

  vec3 p = vec3(position, 0.0);
  //  scale
  p *= global_scale * (1.0 + (inst_rand * 2.0 - 1.0) * 0.125);
  //  curl
  p.z += pow(abs(position.x), 2.0) * curl_scale * global_scale;

  vec3 shadow_p = p;

#ifndef IS_SHADOW
  float osc_scale = 0.125 * pow((position.y * 0.5 + 0.5), 2.0);
  float rate = 1.5 + (inst_rand * 2.0 - 1.0) * 0.25;
  p.z += cos(PI * t * rate + 4.0 * (position.x * 0.5 + 0.5) + inst_rand * PI) * osc_scale;
#endif

  mat3 yrot = y_rotation(aabb_p0.w);
  p = yrot * p;
  shadow_p = yrot * shadow_p;

  mat3 zrot = z_rotation(aabb_p1.w);
  vec3 vrot = zrot * vec3(0.0, 1.5, 0.0);
  p = zrot * p + vrot;
  shadow_p = zrot * shadow_p + vrot;

  p = vec3(p.x, p.z, p.y);
  shadow_p = vec3(shadow_p.x, shadow_p.z, shadow_p.y);

#if 1
  vec3 right = right_global_scale.xyz;
  vec3 up = normalize(cross(forwards, right));
  right = cross(up, forwards);
  mat3 m = mat3(right, up, forwards);
#else
  mat3 m = mat3(right_global_scale.xyz, cross(forwards, right_global_scale.xyz), forwards);
#endif
  p = m * p + translation_curl_scale.xyz;
  shadow_p = m * shadow_p + translation_curl_scale.xyz;

#ifndef IS_SHADOW
  p += wind_displacement(translation_curl_scale.xyz, t);
#endif

#ifndef IS_SHADOW
  vec3 world_center = (aabb_p1.xyz - aabb_p0.xyz) * 0.5 + aabb_p0.xyz;
  vec3 hemi_n = normalize(translation_curl_scale.xyz - vec3(world_center.x, aabb_p0.y, world_center.z));
  v_hemisphere_uv = spherical_to_uv(cartesian_to_spherical(hemi_n)) + uv_off;
  v_uv = position * 0.5 + 0.5;
  v_shadow_position = shadow_p;
  v_normal = normalize(translation_curl_scale.xyz - world_center);
#endif

  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}
