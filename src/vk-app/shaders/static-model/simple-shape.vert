#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;
layout (location = 2) in vec4 scale_active;
layout (location = 3) in vec4 translation;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

layout (location = 0) out vec4 v_color;
layout (location = 1) out float v_active;

void main() {
  v_color = color;
  v_active = scale_active.w;
  gl_Position = projection_view * vec4(scale_active.xyz * position + translation.xyz, 1.0);
}
