#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 v_normalized_position;
layout (location = 1) in vec3 v_position;
layout (location = 2) in vec3 v_position_ls;

#pragma include "color/srgb-to-linear.glsl"
#pragma include "grass/sun-light.glsl"

#pragma include "terrain/set0-uniform-buffer.glsl"

layout (set = 0, binding = 1) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

vec3 get_sun_position() {
  return sun_pos_color_r.xyz;
}

vec3 get_sun_color() {
  return vec3(sun_pos_color_r.w, sun_color_gb_time.xy);
}

vec3 compute_sun_light(vec3 world_pos, float true_y01) {
  const float epsilon = 0.001;
  float diff_ao = max(epsilon, pow(true_y01, 1.5));
  float max_diff = 1.0;
  float max_spec = 1.0;
  return directional_light(get_sun_position(), world_pos, camera_position.xyz, get_sun_color(), max_diff, max_spec, diff_ao);
}

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_position_ls, v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
#if 0
  float noise = step(0.75, v_normalized_position.x);
  noise *= step(-0.25, v_normalized_position.x);

  float y01 = v_normalized_position.y * 0.5 + 0.5;
  y01 *= (1.0 + noise * 0.25);

  float dist_to_center = mod(abs(v_normalized_position.x) + 0.5, 0.5);
  float thresh = pow(y01, 4.0);
#else
  float y01 = v_normalized_position.y * 0.5 + 0.5;
  float dist_to_center = 1.0 - abs(v_normalized_position.x);
  float thresh = pow(y01, 2.0);
#endif
  if (dist_to_center < thresh) {
    discard;
  }

  vec3 green = srgb_to_linear(vec3(173.0/255.0, 1.0, 136.0/255.0));
  vec3 sun_contrib = compute_sun_light(v_position, y01);
  vec3 ground_color = green;
  ground_color += sun_contrib * 0.5;
  ground_color *= max(compute_shadow(), min_shadow_global_color_scale_unused.x);
  ground_color *= min_shadow_global_color_scale_unused.y;
  frag_color = vec4(ground_color, 1.0);

#if 0
  frag_color.xyz = max(vec3(1.0), frag_color.xyz);
#endif
}
