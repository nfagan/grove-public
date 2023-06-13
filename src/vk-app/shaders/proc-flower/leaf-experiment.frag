#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_uv;
layout (location = 1) in vec2 v_hemisphere_uv;
layout (location = 2) in vec3 v_shadow_position;
layout (location = 3) in vec3 v_normal;

layout (set = 0, binding = 1, std140) uniform UniformBuffer {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 shadow_proj_view;
  vec4 camera_position_alpha_test_enabled;
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
  vec4 sun_position;
  vec4 sun_color;
};

#ifdef NO_ALPHA_TEST
layout (set = 0, binding = 2) uniform sampler2D color_image;
layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;
#else
layout (set = 0, binding = 2) uniform sampler2D alpha_test_image;
layout (set = 0, binding = 3) uniform sampler2D color_image;
layout (set = 0, binding = 4) uniform sampler2DArray sun_shadow_texture;
#endif

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"
#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 ls_pos = (shadow_proj_view * vec4(v_shadow_position, 1.0)).xyz;
  vec3 shadow_uvw;
  vec3 cam_pos = camera_position_alpha_test_enabled.xyz;
  return simple_sample_shadow(ls_pos, v_shadow_position, cam_pos, view, sun_shadow_texture, shadow_uvw);
}

vec3 compute_color(vec3 material_info, vec3 color0, vec3 color1, vec3 color2, vec3 color3) {
  vec3 col0 = mix(color0, color1, material_info.r);
  vec3 col1 = mix(color2, color3, material_info.g);
  return mix(col0, col1, material_info.b);
}

void main() {
  float alpha_test_enabled = camera_position_alpha_test_enabled.w;
  float alpha_test_thresh = min(0.4, alpha_test_enabled);

#ifdef NO_ALPHA_TEST
  vec4 material_info = vec4(0.0);
#else
  vec4 material_info = texture(alpha_test_image, v_uv);
  if (material_info.a < alpha_test_thresh) {
    discard;
  }
#endif

  vec3 hemi_color = texture(color_image, v_hemisphere_uv).rgb;
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
}