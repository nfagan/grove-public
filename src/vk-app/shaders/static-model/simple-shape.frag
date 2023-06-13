#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec4 v_color;
layout (location = 1) in float v_active;

#pragma include "color/srgb-to-linear.glsl"

void main() {
  if (v_active == 0.0) {
    discard;
  }
#if 0
  frag_color = v_color;
#else
  frag_color = vec4(srgb_to_linear(v_color.rgb), v_color.a);
#endif
}
