#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (set = 0, binding = 0, std140) uniform UniformData {
  mat4 transform;
};

void main() {
  vec4 trans_pos = transform * vec4(position, 1.0);
  trans_pos.z = (trans_pos.z * 0.5 + 0.5);
  gl_Position = trans_pos;
}
