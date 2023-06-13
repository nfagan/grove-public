#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_position;
layout (location = 1) in vec2 v_uv;

layout (set = 0, binding = 1) uniform sampler2D timeline_content_texture;

void main() {
  vec2 sample_uv = v_uv;
  sample_uv.y = 1.0 - sample_uv.y;
  frag_color = vec4(texture(timeline_content_texture, sample_uv).rgb, 0.75);
}
