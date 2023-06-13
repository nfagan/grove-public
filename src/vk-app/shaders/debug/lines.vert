#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 0) out vec3 v_color;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  v_color = color.xyz;
  gl_Position = projection_view * vec4(position, 1.0);
}
