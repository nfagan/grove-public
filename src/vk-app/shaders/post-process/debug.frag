#version 450

layout (location = 0) out vec4 frag_color;

layout (set = 0, binding = 0) uniform sampler2D scene_color_texture;

void main() {
  frag_color = texelFetch(scene_color_texture, ivec2(gl_FragCoord.xy), 0);
}
