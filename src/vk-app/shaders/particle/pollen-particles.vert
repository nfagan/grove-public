#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 translation_scale;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  vec3 translation = translation_scale.xyz;
  float scale = translation_scale.w;
  gl_Position = projection_view * vec4(position * scale + translation, 1.0);
}
