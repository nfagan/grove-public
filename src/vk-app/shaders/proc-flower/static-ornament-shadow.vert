#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec4 translation_scale;
layout (location = 4) in vec4 direction_y_fraction_uv_scale;

#pragma include "frame.glsl"

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 world_origin_xz_scale;
};

void main() {
  vec3 trans = translation_scale.xyz;
  float instance_scale = translation_scale.w;
  vec2 direction = direction_y_fraction_uv_scale.xy;
  float global_scale = world_origin_xz_scale.z;

  vec3 p = position * instance_scale * global_scale;
  mat3 m = make_coordinate_system_y(spherical_to_cartesian(direction));
  p = m * p + trans;

  vec4 world_pos = vec4(p, 1.0);
  gl_Position = projection_view * world_pos;
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
}
