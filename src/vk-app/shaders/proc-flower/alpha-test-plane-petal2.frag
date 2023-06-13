#version 450

#define DO_ALPHA_TEST (1)

layout (location = 0) in vec2 v_uv;
layout (location = 1) in float v_texture_layer;
layout (location = 2) in vec3 v_light_space_position0;
layout (location = 3) in vec3 v_world_position;
layout (location = 4) flat in uvec4 v_color;

layout (location = 0) out vec4 frag_color;

layout (std140, set = 0, binding = 1) uniform GlobalData {
#pragma include "shadow/sample-struct-fields.glsl"
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 sun_color;
};

layout (set = 0, binding = 3) uniform sampler2DArray material_texture;
layout (set = 0, binding = 4) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_light_space_position0, v_world_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

vec3 unpack_rgb(uint c) {
  const uint m = 0xff;
  vec3 result;
  result.r = float(c & m) / 255.0;
  result.g = float((c >> 8u) & m) / 255.0;
  result.b = float((c >> 16u) & m) / 255.0;
  return result;
}

void unpack_colors(in uvec4 data, out vec3 color0, out vec3 color1, out vec3 color2, out vec3 color3) {
  color0 = unpack_rgb(data.r);
  color1 = unpack_rgb(data.g);
  color2 = unpack_rgb(data.b);
  color3 = unpack_rgb(data.a);
}

vec3 compute_color(vec4 material_info, vec3 color0, vec3 color1, vec3 color2, vec3 color3) {
  vec3 col0 = mix(color0, color1, material_info.r);
  vec3 col1 = mix(color2, color3, material_info.g);
  return mix(col0, col1, material_info.b);
}

void main() {
  vec2 uv = v_uv;
  vec4 material_info = texture(material_texture, vec3(uv, v_texture_layer));
  float alpha = material_info.a;
#if DO_ALPHA_TEST
  if (alpha < 0.4) {
    discard;
  }
#endif
  vec3 color0;
  vec3 color1;
  vec3 color2;
  vec3 color3;
  unpack_colors(v_color, color0, color1, color2, color3);

  vec3 color = compute_color(material_info, color0, color1, color2, color3);
#if 0
  color *= compute_shadow();
#else
  vec3 light = apply_sun_light_shadow(sun_color.rgb, compute_shadow());
  color *= light;
#endif
  frag_color = vec4(color, 1.0);
}