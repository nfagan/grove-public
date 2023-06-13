#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_normal;
layout (location = 1) in vec3 v_position;
layout (location = 2) in vec3 v_position_ls;

#pragma include "color/srgb-to-linear.glsl"
#pragma include "toon-sun-light.glsl"

#pragma include "terrain/set0-uniform-buffer.glsl"

layout (set = 0, binding = 1) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
};

vec3 get_sun_position() {
  return sun_pos_color_r.xyz;
}

vec3 get_sun_color() {
  return vec3(sun_pos_color_r.w, sun_color_gb_time.xy);
}

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_position_ls, v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 n = v_normal;

  n = normalize(n);
  vec3 color = vec3(0.25);

  vec3 sun_light = calculate_sun_light(n, normalize(get_sun_position()), get_sun_color());
  vec3 light_amount = apply_sun_light_shadow(sun_light, compute_shadow());
  color *= light_amount;

  frag_color = vec4(color, 1.0);
}
