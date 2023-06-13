#version 450

layout (location = 0) out vec4 frag_color;

#pragma include "color/srgb-to-linear.glsl"

void main() {
  frag_color = vec4(srgb_to_linear(vec3(0.47, 0.26, 0.02)), 1.0);
}
