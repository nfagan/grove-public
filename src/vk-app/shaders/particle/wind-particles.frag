#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_position;
layout (location = 1) in float v_alpha;

void main() {
  vec3 color = vec3(1.0);
  frag_color = vec4(color, v_alpha);
}
