#version 450

#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

#define NUM_SHADOW_SAMPLES (4)

#pragma include "shadow/data.glsl"

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_position;
layout (location = 1) in vec2 v_uv;
layout (location = 2) in vec3 v_light_space_position0;

#define UNIFORM_DATA_SET (0)
#define UNIFORM_DATA_BINDING (1)
#pragma include "terrain/terrain-data.glsl"

layout (set = 0, binding = 2) uniform sampler2D color_texture;

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (3)
#pragma include "shadow/uniform-buffer.glsl"

layout (set = 0, binding = 4) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/sample.glsl"

#ifdef DDGI_ENABLED

#define SAMPLE_PROBE_SET (0)
#define PROBE_POSITION_INDICES_BINDING (5)
#define PROBE_IRRADIANCE_BINDING (6)
#define PROBE_DEPTH_BINDING (7)
#define PROBE_UNIFORM_BUFFER_BINDING (8)

#pragma include "gi/probe-data.glsl"
#pragma include "gi/oct.glsl"
#pragma include "gi/index.glsl"
#pragma include "gi/sample-probes-data.glsl"
#pragma include "gi/sample-probes.glsl"

#endif

float compute_shadow(out vec3 shadow_uvw) {
  float shadow = simple_sample_shadow(
  v_light_space_position0, v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 shadow_uvw;
  float shadow = compute_shadow(shadow_uvw);

  vec3 tmp_color = texture(color_texture, v_uv).rgb;
  tmp_color *= max(shadow, min_shadow_global_color_scale.x);
  tmp_color *= min_shadow_global_color_scale.y;
#ifdef DDGI_ENABLED
  tmp_color += 4.0 * sample_ddgi_probes(v_position, vec3(0.0, 1.0, 0.0));
#endif

//  if (visualize_shadow_cascades == 1) {
//    tmp_color = compute_shadow_cascade_visualization(tmp_color, shadow_uvw.z);
//  }

  frag_color = vec4(tmp_color, 1.0);
}