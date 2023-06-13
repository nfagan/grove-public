#version 450

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 normal;

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec3 v_position;
layout (location = 2) out vec3 v_position_ls;

#pragma include "terrain/set0-uniform-buffer.glsl"

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  v_normal = normal.xyz;
  v_position = position.xyz;
  v_position_ls = (light_view_projection0 * vec4(position.xyz, 1.0)).xyz;

  gl_Position = projection_view * vec4(position.xyz, 1.0);
}
