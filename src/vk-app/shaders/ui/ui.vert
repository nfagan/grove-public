#version 450

layout (location = 0) in vec2 position;

layout (location = 0) out vec3 v_position;
layout (location = 1) out vec2 v_uv;

layout (set = 0, binding = 0) uniform sampler2D position_texture;

layout (push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  //  vec2 uv = position * 0.5 + 0.5;
  vec2 uv = position;
  vec3 pos = texture(position_texture, uv).rgb;
  v_position = pos;
  v_uv = uv;
  gl_Position = projection_view * vec4(pos, 1.0);
}
