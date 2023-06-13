#version 450

#ifdef IS_VERTEX

layout (location = 0) in uvec4 position_color;
layout (location = 0) out vec3 v_color;

#pragma include "pack/1u32_to_4fn.glsl"

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
};

void main() {
  v_color = pack_1u32_4fn(position_color.w).xyz;
  gl_Position = projection_view * vec4(uintBitsToFloat(position_color.xyz), 1.0);
}

#else

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec3 v_color;

void main() {
  frag_color = vec4(v_color, 1.0);
}

#endif
