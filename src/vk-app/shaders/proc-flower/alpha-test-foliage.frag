#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_uv;
layout (location = 1) in float v_texture_layer;
layout (location = 2) flat in uvec4 v_colors;

layout (set = 0, binding = 1) uniform sampler2DArray material1_image;

#pragma include "proc-flower/material1.glsl"

void main() {
  vec4 mat_info = texture(material1_image, vec3(v_uv, v_texture_layer));

  if (mat_info.a < 0.4) {
    discard;
  }

  vec3 color = material1_color(mat_info, v_colors);
  frag_color = vec4(color, 1.0);
}
