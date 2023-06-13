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

layout (set = 0, binding = 3, std140) uniform UniformBuffer {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 shadow_proj_view;
  vec4 camera_position_alpha_test_enabled;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
  vec4 sun_position;
  vec4 sun_color;
};

#ifdef USE_ARRAY_IMAGES
layout (set = 0, binding = 4) uniform sampler2DArray alpha_test_image;
layout (set = 0, binding = 5) uniform sampler2DArray color_image;
#else
layout (set = 0, binding = 4) uniform sampler2D alpha_test_image;
layout (set = 0, binding = 5) uniform sampler2D color_image;
#endif

layout (set = 0, binding = 6) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"
#pragma include "toon-sun-light.glsl"

vec3 compute_color(vec3 material_info, vec3 color0, vec3 color1, vec3 color2, vec3 color3) {
  vec3 col0 = mix(color0, color1, material_info.r);
  vec3 col1 = mix(color2, color3, material_info.g);
  return mix(col0, col1, material_info.b);
}

float compute_shadow() {
  vec3 ls_pos = (shadow_proj_view * vec4(v_shadow_position, 1.0)).xyz;
  vec3 shadow_uvw;
  vec3 cam_pos = camera_position_alpha_test_enabled.xyz;
  return simple_sample_shadow(ls_pos, v_shadow_position, cam_pos, view, sun_shadow_texture, shadow_uvw);
}

void main() {
//  float alpha_test_thresh = min(0.4, alpha_test_enabled);
  const float alpha_test_thresh = 0.4;
#if 1
  #ifdef USE_ARRAY_IMAGES
  vec4 material_info = texture(alpha_test_image, vec3(v_uv, v_alpha_image_index));
  #else
  vec4 material_info = texture(alpha_test_image, v_uv);
  #endif
  if (material_info.a < alpha_test_thresh) {
    discard;
  }

  #ifdef USE_ARRAY_IMAGES
  vec3 hemi_color = texture(color_image, vec3(v_hemisphere_uv, v_color_image_index)).rgb;
  #else
  vec3 hemi_color = texture(color_image, v_hemisphere_uv).rgb;
  #endif

#ifndef NO_COLOR_MIX
  vec3 color0 = vec3(128, 165, 16) / 255.0;
  vec3 color1 = vec3(187, 80, 27) / 255.0;
  vec3 color2 = vec3(0.0);
  vec3 color3 = vec3(10, 255, 10) / 255.0;
  vec3 mat_color = compute_color(material_info.rgb, color0, color1, color2, color3);
  vec3 color = mix(mat_color, hemi_color, 0.9);
#else
  vec3 color = hemi_color;
#endif

  vec3 light = calculate_sun_light(normalize(v_normal), normalize(sun_position.xyz), sun_color.rgb);
  light = apply_sun_light_shadow(light, compute_shadow());
  color *= light;
  frag_color = vec4(color, 1.0);
#else
  frag_color = vec4(v_scale_frac, 1.0, 1.0, 1.0);
#endif
}
