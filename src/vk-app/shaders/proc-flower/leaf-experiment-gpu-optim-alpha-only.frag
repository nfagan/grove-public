#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_uv;
layout (location = 1) in vec2 v_hemisphere_uv;
layout (location = 2) in vec3 v_normal;
layout (location = 3) in vec3 v_shadow_position;

#ifdef USE_ARRAY_IMAGES
layout (location = 4) in float v_alpha_image_index;
layout (location = 5) in float v_color_image_index;
#endif

#ifdef USE_ARRAY_IMAGES
layout (set = 0, binding = 5) uniform sampler2DArray alpha_test_image;
#else
layout (set = 0, binding = 5) uniform sampler2D alpha_test_image;
#endif

void main() {
  const float alpha_test_thresh = 0.4;
  #ifdef USE_ARRAY_IMAGES
  vec4 material_info = texture(alpha_test_image, vec3(v_uv, v_alpha_image_index));
  #else
  vec4 material_info = texture(alpha_test_image, v_uv);
  #endif
  if (material_info.a < alpha_test_thresh) {
    discard;
  }

  frag_color = vec4(1.0);
}
