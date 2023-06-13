#version 450

//#define USE_ORIGINAL_SUN_LIGHT

layout (location = 0) out vec4 color;

#define GRASS_UNIFORM_SET (0)
#define GRASS_UNIFORM_BINDING (0)
#pragma include "grass/grass-data.glsl"

#define NUM_SHADOW_SAMPLES (4)
#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

#pragma include "grass/sun-light.glsl"

layout (location = 0) in VS_OUT vs_out;

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;
layout (set = 0, binding = 5) uniform sampler2D terrain_color_texture;

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

const vec3 up = vec3(0.0, 1.0, 0.0);

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_out.light_space_position0, vs_out.v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

vec3 compute_sun_light() {
  const float epsilon = 0.0001;
  float diff_ao = max(epsilon, pow(vs_out.v_y, 1.5));
#ifdef USE_ORIGINAL_SUN_LIGHT
  return directional_light(
    sun_position.xyz, vs_out.v_position, camera_position.xyz, sun_color.xyz, diff_ao);
#else
  float max_diffuse = terrain_grid_scale_max_diffuse_max_specular.y;
  float max_spec = terrain_grid_scale_max_diffuse_max_specular.z;
  return directional_light(
    sun_position.xyz, vs_out.v_position, camera_position.xyz, sun_color.xyz, max_diffuse, max_spec, diff_ao);
#endif
}

void main() {
  bool discard_at_edge = min_shadow_global_color_scale_discard_at_edge.z > 0.0;
  if (discard_at_edge && vs_out.v_alpha < 0.99999) {
    discard;
  } else if (!discard_at_edge && vs_out.v_alpha >= 0.99999) {
    discard;
  }

  float min_shadow = min_shadow_global_color_scale_discard_at_edge.x;
  float global_color_scale = min_shadow_global_color_scale_discard_at_edge.y;

  vec3 tmp_color = texture(terrain_color_texture, vs_out.color_uv).rgb;
  vec3 sun_contrib = compute_sun_light();

  tmp_color += sun_contrib * 0.5;
  tmp_color *= max(compute_shadow(), min_shadow);
  tmp_color *= global_color_scale;
#ifdef DDGI_ENABLED
  tmp_color += 4.0 * sample_ddgi_probes(vs_out.v_position, up);
#endif

  color = vec4(tmp_color, vs_out.v_alpha);
}