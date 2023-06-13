#version 450

#define DO_ALPHA_TEST (1)

layout (location = 0) in vec2 v_alpha_uv;
layout (location = 1) in vec2 v_color_uv;
layout (location = 2) in float v_texture_layer;
layout (location = 3) in vec3 v_light_space_position0;
layout (location = 4) in vec3 v_world_position;
layout (location = 5) flat in uvec4 v_color;

layout (location = 0) out vec4 frag_color;

#ifdef IS_BRANCH_WIND
#define UN_BINDING (2)
#define ALPHA_TEX_BINDING (4)
#define SHADOW_TEX_BINDING (5)
#else
#define UN_BINDING (1)
#define ALPHA_TEX_BINDING (3)
#define SHADOW_TEX_BINDING (4)
#endif

layout (std140, set = 0, binding = UN_BINDING) uniform GlobalData {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 sun_color;
};

layout (set = 0, binding = ALPHA_TEX_BINDING) uniform sampler2DArray alpha_texture;
layout (set = 0, binding = SHADOW_TEX_BINDING) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

#pragma include "toon-sun-light.glsl"
#pragma include "orn-foliage/material1.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_light_space_position0, v_world_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
#ifdef ENABLE_ALPHA_TO_COV
  const float alpha_test_thresh = 0.01;
#else
  const float alpha_test_thresh = 0.4;
#endif

  vec4 material_info = texture(alpha_texture, vec3(v_alpha_uv, v_texture_layer));

  float alpha = material_info.a;
#ifdef ENABLE_ALPHA_TO_COV
  //  https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f
  alpha = (alpha - alpha_test_thresh) / max(fwidth(alpha), 0.0001) + 0.5;
#endif

#if DO_ALPHA_TEST
  if (alpha < alpha_test_thresh) {
    discard;
  }
#endif

  vec3 color = material1_color(material_info, v_color);
  vec3 light = apply_sun_light_shadow(sun_color.rgb, compute_shadow());
  color *= light;
#ifdef ENABLE_ALPHA_TO_COV
  frag_color = vec4(color, alpha);
#else
  frag_color = vec4(color, 1.0);
#endif
}