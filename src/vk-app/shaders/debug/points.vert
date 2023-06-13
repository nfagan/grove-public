#version 450

layout (location = 0) in vec3 position;
layout (location = 0) out vec3 v_color;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 color_point_size;
};

void main() {
  v_color = color_point_size.xyz;
  gl_PointSize = color_point_size.w;
  gl_Position = projection_view * vec4(position, 1.0);
}