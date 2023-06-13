#version 450

#define NO_PCF

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)
#pragma include "grass/alt-grass-data.glsl"

#define NUM_SHADOW_SAMPLES (4)
#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (4)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

#define NEW_MAT_BINDING (6)

layout (set = 0, binding = 5) uniform sampler2DArray sun_shadow_texture;

layout (location = 0) out vec4 frag_color;
layout (location = 0) in VS_OUT vs_in;

#pragma include "pi.glsl"
#pragma include "grass/new-material.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_world_pos, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
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

  float min_shadow = min_shadow_global_color_scale.x;
  float splotch = vs_in.v_world_uv.x;
  float shadow = max(compute_shadow(), min_shadow);
  vec3 v = normalize(camera_position.xyz - vs_in.v_world_pos);
  float spec_atten = clamp(1.0 - vs_in.v_sun_fade_out_frac, 0.0, 1.0);

  vec3 tmp_color = new_material_color(clamp(true_y01, 0.0, 1.0), splotch, v, sun_position.xyz, sun_color.xyz, shadow, make_new_material_params(spec_atten));
  frag_color = vec4(tmp_color, vs_in.v_alpha);
}