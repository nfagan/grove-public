#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (push_constant) uniform PushConstantData {
  mat4 proj_view;
  vec4 translation_scale;
};

void main() {
  gl_Position = proj_view * vec4(position * translation_scale.w + translation_scale.xyz, 1.0);
  gl_Position.z = gl_Position.z * 0.5 + 0.5;
}
