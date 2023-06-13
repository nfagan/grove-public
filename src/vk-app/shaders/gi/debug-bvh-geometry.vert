#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in uint texture_layer;

layout (location = 0) out vec2 v_uv;
layout (location = 1) flat out uint v_texture_layer;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  v_uv = uv;
  v_texture_layer = texture_layer;
  gl_Position = projection_view * vec4(position, 1.0);
}
