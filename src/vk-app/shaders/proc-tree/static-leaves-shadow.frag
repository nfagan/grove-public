#version 450

layout (set = 0, binding = 0) uniform sampler2D material_texture;
layout (location = 0) in vec2 v_uv;

void main() {
  vec4 sampled = texture(material_texture, v_uv);
  if (sampled.a < 0.5 || length(v_uv * 2.0 - 1.0) > 1.0) {
    discard;
  }
}