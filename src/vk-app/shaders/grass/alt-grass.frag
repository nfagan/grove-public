#version 450

//#define USE_ORIGINAL_SUN_LIGHT

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)
#pragma include "grass/alt-grass-data.glsl"

#define NUM_SHADOW_SAMPLES (4)
#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (4)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

layout (set = 0, binding = 5) uniform sampler2DArray sun_shadow_texture;
layout (set = 0, binding = 6) uniform sampler2D ground_color_texture;

layout (location = 0) out vec4 frag_color;
layout (location = 0) in VS_OUT vs_in;

#pragma include "grass/sun-light.glsl"
#pragma include "pi.glsl"

#ifdef DDGI_ENABLED

#define SAMPLE_PROBE_SET (0)
#define PROBE_POSITION_INDICES_BINDING (7)
#define PROBE_IRRADIANCE_BINDING (8)
#define PROBE_DEPTH_BINDING (9)
#define PROBE_UNIFORM_BUFFER_BINDING (10)

#pragma include "gi/probe-data.glsl"
#pragma include "gi/oct.glsl"
#pragma include "gi/index.glsl"
#pragma include "gi/sample-probes-data.glsl"
#pragma include "gi/sample-probes.glsl"

#endif

float calculate_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_world_pos, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

vec3 compute_sun_light(vec3 world_pos, float true_y01) {
  const float epsilon = 0.001;
  float diff_ao = max(epsilon, pow(true_y01, 1.5));
#ifdef USE_ORIGINAL_SUN_LIGHT
  return directional_light(sun_position.xyz, world_pos, camera_position.xyz, sun_color.xyz, diff_ao);
#else
  float max_diff = time_max_diffuse_max_specular.y;
  float max_spec = time_max_diffuse_max_specular.z;
  return directional_light(sun_position.xyz, world_pos, camera_position.xyz, sun_color.xyz, max_diff, max_spec, diff_ao);
#endif
}

void main() {
  const float num_blades = 1.0;
  const float blade_span = 1.0 / num_blades;

  float true_y01 = vs_in.v_normalized_position.y;
  float y01 = pow(true_y01, 3.0 + vs_in.v_rand * 0.5);
  float x01 = vs_in.v_normalized_position.x;
  float x11 = x01 * 2.0 - 1.0;
  float x_displace = 0.0;

  float blade_index = floor((x01 + x_displace) / blade_span);
  float blade_index_frac = blade_index / max(1.0, num_blades - 1.0);
  float blade_rand01 = sin(blade_index * 8192.0 + vs_in.v_rand * 2.0 * PI) * 0.5 + 0.5;
  float blade_rand11 = blade_rand01 * 2.0 - 1.0;

  float local_blade_scale = 1.0;
  vec3 world_pos = vs_in.v_world_pos;
  vec2 world_uv = vs_in.v_world_uv;

  const float min_y_mul = 0.0;
  const float min_dist_thresh = 0.0;

  float blade_rel = mod(x01 + x_displace, blade_span);
  float center_rel = abs(blade_rel - blade_span * 0.5);
  float dist_thresh = max(min_dist_thresh, local_blade_scale * blade_span * 0.5 * max(min_y_mul, (1.0 - y01)));

  float min_lim = 0.0;
  float frac_thresh = (clamp(center_rel / dist_thresh, min_lim, 1.0) - min_lim) / (1.0 - min_lim);
  float alpha_taper = 1.0 - frac_thresh;

  if (alpha_taper < 0.001) {
    discard;
  }

  vec3 ground_color = texture(ground_color_texture, world_uv).rgb;
  vec3 sun_contrib = compute_sun_light(world_pos, true_y01);

  ground_color += sun_contrib * 0.5 * (1.0 - vs_in.v_sun_fade_out_frac);
  ground_color *= max(calculate_shadow(), min_shadow_global_color_scale.x);
  ground_color *= min_shadow_global_color_scale.y;
#ifdef DDGI_ENABLED
  ground_color += 4.0 * sample_ddgi_probes(world_pos, vec3(0.0, 1.0, 0.0));
#endif

  //  frag_color = vec4(0.0, blade_index_frac, 0.0, alpha_taper);
  frag_color = vec4(ground_color, vs_in.v_alpha);
}