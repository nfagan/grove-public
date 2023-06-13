#version 450

#ifdef IS_VERTEX

layout (location = 0) in vec4 position_uv;
layout (location = 1) in uvec4 texture_layer_color_unused;

layout (location = 0) flat out uint v_texture_layer;
layout (location = 1) out vec2 v_uv;
layout (location = 2) out vec3 v_color;

layout (std140, push_constant) uniform PushConstantData {
  vec4 framebuffer_dimensions;
};

#pragma include "pack/1u32_to_4fn.glsl"

void main() {
#if 0
  //  @NOTE: 03/18/22 - Why did we floor here? This produces blurry / misaligned glyphs
  vec2 xy = floor(position_uv.xy + vec2(0.5)) / framebuffer_dimensions.xy * 2.0 - 1.0;
#else
  vec2 xy = (position_uv.xy + vec2(0.5)) / framebuffer_dimensions.xy * 2.0 - 1.0;
#endif

  v_uv = position_uv.zw;
  v_texture_layer = texture_layer_color_unused.x;
  v_color = pack_1u32_4fn(texture_layer_color_unused.y).xyz;

  gl_Position = vec4(vec2(1.0, 1.0) * xy, 0.0, 1.0);
}

#else

layout (location = 0) out vec4 frag_color;

layout (location = 0) flat in uint v_texture_layer;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in vec3 v_color;

layout (set = 0, binding = 0) uniform sampler2DArray glyph_image;

void main() {
  float a = texture(glyph_image, vec3(v_uv, float(v_texture_layer))).r;
  frag_color = vec4(v_color, a);
}

#endif
