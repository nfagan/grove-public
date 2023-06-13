#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_uv;

layout (set = 0, binding = 0) uniform sampler2D timeline_content_texture;

void main() {
  vec2 sample_uv = vec2(v_uv.y, v_uv.x);
  sample_uv.x = 1.0 - sample_uv.x;
  frag_color = vec4(texture(timeline_content_texture, sample_uv).rgb, 0.75);
}
