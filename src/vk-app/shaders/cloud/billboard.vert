#version 450 core

layout (location = 0) in vec3 position;

layout (location = 0) out vec4 v_proj_position;
layout (location = 1) out vec2 v_uv;

#pragma include "cloud/billboard-data.glsl"

void main() {
  vec3 scale = scale_depth_test_enable.xyz;
  vec3 translation = translation_opacity_scale.xyz;

  mat3 inv_view;
  inv_view[0] = vec3(camera_right_front.x, 0.0, camera_right_front.y);
  inv_view[1] = vec3(0.0, 1.0, 0.0);
  inv_view[2] = vec3(camera_right_front.z, 0.0, camera_right_front.w);

  vec3 world_pos = inv_view * (vec3(position.x, position.y, 0.0) * scale) + translation;
  vec4 proj_pos = projection_view * vec4(world_pos, 1.0);

  v_proj_position = proj_pos;
  v_uv = position.xy * 0.5 + 0.5;

  gl_Position = proj_pos;
}