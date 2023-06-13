#version 450

struct RenderInstance {
  vec4 translation_forwards_x;
  vec4 forwards_yz_right_xy;
  uvec4 right_z_instance_group_randomness_unused;
  vec4 y_rotation_z_rotation_unused;
  uvec4 wind_node_info0;
  uvec4 wind_node_info1;
  uvec4 wind_node_info2;
};

struct RenderInstanceGroup {
  uvec4 alpha_image_color_image_indices_uv_offset_unused;
  vec4 aabb_p0_curl_scale;
  vec4 aabb_p1_global_scale;
};

struct LODDependentData {
  vec4 scale_fraction_lod_fraction;
};

layout (location = 0) in vec2 position;
layout (location = 1) in uint instance_index;

#ifndef IS_SHADOW
layout (location = 0) out vec2 v_uv;
layout (location = 1) out vec2 v_hemisphere_uv;
layout (location = 2) out vec3 v_normal;
layout (location = 3) out vec3 v_shadow_position;

#ifdef USE_ARRAY_IMAGES
layout (location = 4) out float v_alpha_image_index;
layout (location = 5) out float v_color_image_index;
#endif

#endif

layout (set = 0, binding = 0, std430) readonly buffer Instances {
  RenderInstance instances[];
};

#ifndef IS_SHADOW
layout (set = 0, binding = 1, std430) readonly buffer LODDependentDatas {
  LODDependentData lod_dependent_data[];
};

layout (set = 0, binding = 2, std430) readonly buffer RenderInstanceGroups {
  RenderInstanceGroup instance_groups[];
};

layout (set = 0, binding = 3, std140) uniform UniformBuffer {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 shadow_proj_view;
  vec4 camera_position_alpha_test_enabled;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
  vec4 sun_position;
  vec4 sun_color;
};

#else
layout (set = 0, binding = 1, std430) readonly buffer RenderInstanceGroups {
  RenderInstanceGroup instance_groups[];
};
#endif

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 data0;
};

vec3 get_forwards(in RenderInstance inst) {
  return vec3(inst.translation_forwards_x.w, inst.forwards_yz_right_xy.xy);
}

vec3 get_right(in RenderInstance inst) {
  return vec3(inst.forwards_yz_right_xy.zw, uintBitsToFloat(inst.right_z_instance_group_randomness_unused.x));
}

float get_randomness(in RenderInstance inst) {
  return uintBitsToFloat(inst.right_z_instance_group_randomness_unused.z);
}

float get_y_rotation(in RenderInstance inst) {
  return inst.y_rotation_z_rotation_unused.x;
}

float get_z_rotation(in RenderInstance inst) {
  return inst.y_rotation_z_rotation_unused.y;
}

float get_uv_offset(in RenderInstanceGroup inst_group) {
  //  @NOTE: x is packed u16
  return uintBitsToFloat(inst_group.alpha_image_color_image_indices_uv_offset_unused.y);
}

float get_alpha_image_index(in RenderInstanceGroup inst_group) {
  return float(inst_group.alpha_image_color_image_indices_uv_offset_unused.x & 0xffffu);
}

float get_color_image_index(in RenderInstanceGroup inst_group) {
  return float((inst_group.alpha_image_color_image_indices_uv_offset_unused.x >> 16u) & 0xffffu);
}

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "spherical-to-uv.glsl"
#pragma include "x_rotation.glsl"
#pragma include "y_rotation.glsl"
#pragma include "z_rotation.glsl"

vec3 get_aabb_p0(in RenderInstanceGroup inst_group) {
  return inst_group.aabb_p0_curl_scale.xyz;
}

vec3 get_aabb_p1(in RenderInstanceGroup inst_group) {
  return inst_group.aabb_p1_global_scale.xyz;
}

vec3 get_world_center(vec3 aabb_p0, vec3 aabb_p1) {
  return (aabb_p1 - aabb_p0) * 0.5 + aabb_p0;
}

vec2 get_hemisphere_uv(in RenderInstanceGroup inst_group, vec3 translation, vec3 world_center) {
  vec3 hemi_n = normalize(translation - vec3(world_center.x, inst_group.aabb_p0_curl_scale.y, world_center.z));
  return spherical_to_uv(cartesian_to_spherical(hemi_n));
}

void main() {
  float t = data0.x;

  RenderInstance inst = instances[instance_index];
#ifndef IS_SHADOW
  LODDependentData lod_inst = lod_dependent_data[instance_index];
#endif

  uint instance_group_index = inst.right_z_instance_group_randomness_unused.y;
  RenderInstanceGroup inst_group = instance_groups[instance_group_index];

  vec3 forwards = get_forwards(inst);
  vec3 right = get_right(inst);
  float inst_rand = get_randomness(inst);
  float uv_off = get_uv_offset(inst_group);

  vec3 up = normalize(cross(forwards, right));
  right = cross(up, forwards);
  mat3 m = mat3(right, up, forwards);

  vec3 translation = inst.translation_forwards_x.xyz;
  float curl_scale = inst_group.aabb_p0_curl_scale.w;
  float global_scale = inst_group.aabb_p1_global_scale.w;

#ifndef IS_SHADOW
  global_scale *= lod_inst.scale_fraction_lod_fraction.x;
  curl_scale *= lod_inst.scale_fraction_lod_fraction.y;
#else
  curl_scale = 0.0;
#endif

  vec3 p = vec3(position, 0.0);
  //  scale
  p *= global_scale * (1.0 + (inst_rand * 2.0 - 1.0) * 0.125);
  //  curl
#ifndef IS_SHADOW
  p.z += pow(abs(position.x), 2.0) * curl_scale * global_scale;
#endif

  vec3 shadow_p = p;

#ifndef IS_SHADOW
  float osc_scale = 0.125 * pow((position.y * 0.5 + 0.5), 2.0);
  float rate = 1.5 + (inst_rand * 2.0 - 1.0) * 0.25;
  p.z += cos(PI * t * rate + 4.0 * (position.x * 0.5 + 0.5) + inst_rand * PI) * osc_scale;
#endif

  mat3 yrot = y_rotation(get_y_rotation(inst));
  p = yrot * p;
  shadow_p = yrot * shadow_p;

  mat3 zrot = z_rotation(get_z_rotation(inst));
  vec3 vrot = zrot * vec3(0.0, 1.5, 0.0);
  p = zrot * p + vrot;
  shadow_p = zrot * shadow_p + vrot;

  p = vec3(p.x, p.z, p.y);
  shadow_p = vec3(shadow_p.x, shadow_p.z, shadow_p.y);

  p = m * p + translation;
  shadow_p = m * shadow_p + translation;

#ifndef IS_SHADOW
  vec3 aabb_p0 = get_aabb_p0(inst_group);
  vec3 aabb_p1 = get_aabb_p1(inst_group);
  vec3 world_center = get_world_center(aabb_p0, aabb_p1);
//  p += wind_displacement(inst, aabb_p0, aabb_p1, world_center, translation, t);

  v_uv = position * 0.5 + 0.5;
  v_normal = normalize(translation - world_center);
  v_hemisphere_uv = get_hemisphere_uv(inst_group, translation, world_center) + uv_off;
  v_shadow_position = shadow_p;

  #ifdef USE_ARRAY_IMAGES
  v_alpha_image_index = get_alpha_image_index(inst_group);
  v_color_image_index = get_color_image_index(inst_group);
  #endif

#endif
  gl_Position = projection_view * vec4(p, 1.0);
#ifdef IS_SHADOW
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
#endif
}
