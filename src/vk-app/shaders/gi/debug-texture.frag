#version 450 core

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec2 v_uv;

layout (set = 0, binding = 0) uniform sampler2D probe_texture;

void main() {
  frag_color = vec4(texture(probe_texture, v_uv).rgb, 1.0);
}
