#version 450

layout (location = 0) out vec4 lo;
layout (location = 0) in vec2 v_uv;

layout (set = 0, binding = 0) uniform sampler2D sky_color_texture;
layout (set = 0, binding = 1) uniform sampler2D bayer_texture;

#pragma include "color/srgb-to-linear.glsl"

void main() {
  lo = vec4(texture(sky_color_texture, v_uv).rgb, 1.0);
#if 1
  //  https://stackoverflow.com/questions/16005952/opengl-gradient-banding-artifacts
  float bayer = texture(bayer_texture, gl_FragCoord.xy / 8.0).r * (255.0 / 64.0);
  const float rgb_byte_max = 255.0;
  vec3 rgb = rgb_byte_max * lo.rgb;
  vec3 head = floor(rgb);
  vec3 tail = rgb - head;
  lo.rgb = head + step(bayer, tail);
  lo.rgb /= rgb_byte_max;
  lo.rgb = srgb_to_linear(lo.rgb);
#endif
}
