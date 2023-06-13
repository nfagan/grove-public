#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec2 v_uv;

layout (push_constant) uniform PushConstantData {
  mat4 view;
  mat4 projection;
};

void main() {
  v_uv = clamp(uv, vec2(0.05), vec2(1.0));
  mat4 v = mat4(mat3(view));
  gl_Position = projection * v * vec4(position, 1.0);
  gl_Position.z = 0.0;
}
