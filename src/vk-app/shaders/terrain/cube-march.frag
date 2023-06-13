#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_normal;
layout (location = 1) in vec3 v_position;
layout (location = 2) in vec3 v_position_ls;

#pragma include "color/srgb-to-linear.glsl"
#pragma include "toon-sun-light.glsl"

#pragma include "terrain/set0-uniform-buffer.glsl"

layout (set = 0, binding = 1) uniform sampler2DArray sun_shadow_texture;
layout (set = 0, binding = 2) uniform sampler2D splotch_image;
layout (set = 0, binding = 3) uniform sampler2D ground_color_image;

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

float sample_splotch() {
  vec2 uv = v_position.xz * 0.05;
//  vec2 uv = v_position.xz * 0.2;
  return texture(splotch_image, uv).r;
}

vec3 sample_ground_color() {
  vec2 uv = v_position.xz * 0.05;
  return texture(ground_color_image, uv).rgb;
}

void main() {
  vec3 n = v_normal;

  n = normalize(n);

  float splotch = sample_splotch();
//  float ground_mix = clamp(n.y + splotch * 0.25, 0.0, 1.0);
  float ground_mix = clamp(pow(clamp(n.y, 0.0, 1.0), 2.0) - splotch * 0.25, 0.0, 1.0);
//  float ground_mix = clamp(splotch * 1.45, 0.0, 1.0);

//  vec3 c1 = srgb_to_linear(vec3(0.47, 0.26, 0.02));
  vec3 c1 = vec3(0.025, 0.025, 0.04);
//  vec3 c1 = vec3(0.25);
//  vec3 c0 = vec3(0.0, 1.0, 0.0);
//  vec3 c0 = srgb_to_linear(vec3(173.0/255.0, 1.0, 136.0/255.0));
  vec3 c0 = c1 * 0.75;
  float t = 1.0 - pow(ground_mix, 0.75);
//  t = step(0.5, t);
  vec3 color = mix(c0, c1, t);

  vec3 green = srgb_to_linear(vec3(173.0/255.0, 1.0, 136.0/255.0));
  color = mix(color, green, 0.018);
  color = vec3(0.25);

#if 1
//  vec3 ground_color = green;
  vec3 ground_color = sample_ground_color();

  color = mix(color, ground_color, step(0.8, pow(ground_mix, 2.0)));
//  color = mix(color, ground_color, step(0.95, pow(ground_mix, 8.0)));
//  color = mix(color, ground_color, pow(smoothstep(0.0, 1.0, ground_mix), 16.0));
#endif

  vec3 sun_light = calculate_sun_light(n, normalize(get_sun_position()), get_sun_color());
  vec3 light_amount = apply_sun_light_shadow(sun_light, compute_shadow());
  color *= light_amount;

  frag_color = vec4(color, 1.0);
//  frag_color = vec4(n, 1.0);
}
