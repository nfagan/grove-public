#version 450

#ifndef NUM_IMAGE_COMPONENTS
#error "Expected NUM_IMAGE_COMPONENTS define"
#endif

layout (location = 0) out vec4 frag_color;
layout (location = 0) in vec2 v_uv;
layout (location = 1) in float v_min_alpha;
layout (set = 0, binding = 0) uniform sampler2D source_image;

void main() {
#if NUM_IMAGE_COMPONENTS == 1
  float sampled = texture(source_image, v_uv).r;
  frag_color = vec4(sampled, 0.0, 0.0, max(1.0, v_min_alpha));

#elif NUM_IMAGE_COMPONENTS == 2
  vec2 sampled = texture(source_image, v_uv).rg;
  frag_color = vec4(sampled, 0.0, max(1.0, v_min_alpha));

#elif NUM_IMAGE_COMPONENTS == 3
  vec3 sampled = texture(source_image, v_uv).rgb;
  frag_color = vec4(sampled, max(1.0, v_min_alpha));

#elif NUM_IMAGE_COMPONENTS == 4
  vec4 sampled = texture(source_image, v_uv);
  frag_color = vec4(sampled.rgb, max(sampled.a, v_min_alpha));
#else
  #error "Expected image components in range [1, 4]"
#endif
}
