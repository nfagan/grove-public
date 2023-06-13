#version 450

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)

#define INSTANCE_UNIFORM_SET (1)
#define INCLUDE_INSTANCE_TRANSLATION

#pragma include "proc-flower/static-ornament-data.glsl"

layout (set = GLOBAL_UNIFORM_SET, binding = 1) uniform sampler2D wind_displacement_texture;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec4 translation_scale;
layout (location = 4) in vec4 direction_y_fraction_uv_scale;

layout (location = 0) out VS_OUT vs_out;

#pragma include "pi.glsl"
#pragma include "frame.glsl"
#pragma include "wind.glsl"
#pragma include "proc-flower/wind.glsl"
#pragma include "rotation2.glsl"
#pragma include "y_rotation.glsl"

layout (push_constant) uniform PushConstantData {
  vec4 world_origin_xz_scale;
};

vec2 sample_wind(vec2 world_xz) {
  return sample_wind_tip_displacement(world_xz, wind_world_bound_xz, wind_displacement_texture);
}

void main() {
  vec3 trans = translation_scale.xyz;
  float instance_scale = translation_scale.w;
  vec2 direction = direction_y_fraction_uv_scale.xy;
  float y_fraction = direction_y_fraction_uv_scale.z;
  float uv_scale = direction_y_fraction_uv_scale.w;
  vec2 origin_xz = world_origin_xz_scale.xy;
  float global_scale = world_origin_xz_scale.z;

  float t = time_info.x;
  vec2 sampled_wind = sample_wind(origin_xz);

  vec3 p = position * instance_scale * global_scale;
  mat3 m = make_coordinate_system_y(spherical_to_cartesian(direction));
#if 1
  m = m * y_rotation(sin(trans.y * trans.x * trans.z * 256.0) * PI);
#endif
  p = m * p + trans;
  vec3 p_no_wind = p;
  //  wind
  p.xz += wind_displacement(t, y_fraction, sampled_wind, origin_xz);

  vec4 world_pos = vec4(p, 1.0);
  vec4 shadow_world_pos = vec4(p_no_wind, 1.0);

  vec2 v_uv = uv * uv_scale;
  float uv_disp_strength = 0.025;
  float theta = sin(p.y * 8.0 + t * 4.0) * uv_disp_strength * PI;
  mat2 uv_rot = rotation2(theta);
  v_uv = uv_rot * v_uv;

  vs_out.v_uv = v_uv;
  vs_out.v_light_space_position0 = (sun_light_view_projection0 * shadow_world_pos).xyz;
  vs_out.v_world_position = world_pos.xyz;
  vs_out.v_normal = m * normal;
  vs_out.v_instance_translation = trans;

  gl_Position = projection * view * world_pos;
}