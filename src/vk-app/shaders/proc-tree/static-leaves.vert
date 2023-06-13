#version 450

#pragma include "proc-tree/static-leaves-data.glsl"

layout (set = 0, binding = 1) uniform sampler2D wind_displacement_texture;

layout (location = 0) in vec4 position_u;
layout (location = 1) in vec4 normal_v;
layout (location = 2) in vec4 instance_position_transform_buffer_index;
layout (location = 3) in vec2 instance_direction;
layout (location = 4) in uvec4 packed_axis_root_info0;
layout (location = 5) in uvec4 packed_axis_root_info1;
layout (location = 6) in uvec4 packed_axis_root_info2;

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "x_rotation.glsl"
#pragma include "wind.glsl"
#pragma include "proc-tree/wind.glsl"
#pragma include "proc-tree/wind-displacement.glsl"
#pragma include "rotation2.glsl"

#ifdef INCLUDE_HEMISPHERE_UV

#pragma include "spherical-to-uv.glsl"

#endif

layout (push_constant, std140) uniform PushConstantData {
  vec4 aabb_p0_color_r;
  vec4 aabb_p1_color_g;
  vec4 model_scale_color_b;
  vec4 wind_fast_osc_scale_unused;  //  float, unused ...
};

vec3 compute_aabb_center(vec3 aabb_p0, vec3 aabb_p1) {
  return (aabb_p1 - aabb_p0) * 0.5 + aabb_p0;
}

vec2 sample_wind(vec2 world_xz) {
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

void main() {
  vec2 uv = vec2(position_u.w, normal_v.w);
  vec3 position = position_u.xyz;
  vec3 normal = normal_v.xyz;

  //  global uniform
  float t = time_info.x;
  //  end global

  //  instance uniform
  vec3 aabb_p0 = aabb_p0_color_r.xyz;
  vec3 aabb_p1 = aabb_p1_color_g.xyz;
  vec3 model_scale = model_scale_color_b.xyz;
  vec3 world_center = compute_aabb_center(aabb_p0, aabb_p1);
  float wind_fast_osc_amp = wind_fast_osc_scale_unused.x;

//  model_scale += wind_fast_osc_amp;
  //  end instance

  vec2 sampled_wind = sample_wind(world_center.xz);
  float wind_atten = wind_attenuation(
    sampled_wind, wind_displacement_limits_wind_strength_limits.xy, wind_displacement_limits_wind_strength_limits.zw);

  vec4 root_info0;
  vec4 root_info1;
  vec4 root_info2;
  vec4 child_root_info0;
  vec4 child_root_info1;
  vec4 child_root_info2;
  unpack_axis_root_info(packed_axis_root_info0, root_info0, child_root_info0);
  unpack_axis_root_info(packed_axis_root_info1, root_info1, child_root_info1);
  unpack_axis_root_info(packed_axis_root_info2, root_info2, child_root_info2);

  vec3 instance_pos = instance_position_transform_buffer_index.xyz;

  mat3 m = make_coordinate_system_y(spherical_to_cartesian(instance_direction));
  vec3 pos = m * (position * model_scale) + instance_pos;
  vec3 displaced_pos = wind_displacement(instance_pos, t, aabb_p0, aabb_p1, root_info0, root_info1, root_info2, wind_atten);
  vec3 pos_wind = pos + (displaced_pos - instance_pos);
  //  No wind
  vec3 shadow_pos = pos;

  vs_out.v_uv = uv;
  vs_out.v_light_space_position0 = vec3(sun_light_view_projection0 * vec4(shadow_pos, 1.0));
  vs_out.v_shadow_position = shadow_pos;
#if 0
  vs_out.v_normal = m * normal;
#else
  vs_out.v_normal = normalize(instance_pos - world_center);

//  const float uv_disp_strength = 0.025;
  const float uv_disp_strength = 0.1;
  float uv_theta = sin(pos.y * 12.0 + t * 4.5) * uv_disp_strength * PI * length(sampled_wind);
//  uv_theta += wind_fast_osc_scale_unused.y * 0.5; //  can be (ab)used to simulate leaf death
//  uv_theta += PI * wind_fast_osc_amp * sin(pos_wind.y * 8.0);

  mat2 uv_rot = rotation2(uv_theta);
  vs_out.v_uv = uv_rot * uv;
#endif
#ifdef INCLUDE_HEMISPHERE_UV
  vec3 hemi_n = normalize(instance_pos - vec3(world_center.x, aabb_p0.y, world_center.z));
  vs_out.v_hemisphere_uv = spherical_to_uv(cartesian_to_spherical(hemi_n));
  vs_out.v_hemisphere_uv += mod(wind_fast_osc_scale_unused.y * 0.01 + wind_fast_osc_scale_unused.z, 1.0);
#endif

  gl_Position = projection * view * vec4(pos_wind, 1.0);
}