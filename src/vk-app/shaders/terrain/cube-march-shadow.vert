#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (std140, push_constant) uniform PushConstantData {
  mat4 light_view_projection;
};

void main() {
  vec4 trans_pos = light_view_projection * vec4(position, 1.0);
  trans_pos.z = (trans_pos.z * 0.5 + 0.5);
  gl_Position = trans_pos;
}