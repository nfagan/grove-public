#version 450

layout (location = 0) out vec4 frag_color;
layout (set = 0, binding = 0) uniform sampler2D scene_color_texture;

#ifdef SAMPLE_LINEAR
layout (location = 0) in vec2 v_uv;
#endif

void main() {
#ifdef SAMPLE_LINEAR
  frag_color = texture(scene_color_texture, v_uv);
#else
  frag_color = texelFetch(scene_color_texture, ivec2(gl_FragCoord.xy), 0);
#endif
}
