#version 450

//#define NO_PCF

#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

#define NUM_SHADOW_SAMPLES (4)

#pragma include "shadow/data.glsl"

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_position;
layout (location = 1) in float v_splotch;
layout (location = 2) in vec3 v_light_space_position0;

#define UNIFORM_DATA_SET (0)
#define UNIFORM_DATA_BINDING (1)
#pragma include "terrain/terrain-data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (3)
#pragma include "shadow/uniform-buffer.glsl"

layout (set = 0, binding = 4) uniform sampler2DArray sun_shadow_texture;

#define NEW_MAT_BINDING (5)

#pragma include "grass/new-material.glsl"
#pragma include "shadow/sample.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_light_space_position0, v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  if (length(v_position) > 256.0 - 16.0) {
    discard;
  }

  float min_shadow = min_shadow_global_color_scale.x;
  float shadow = max(compute_shadow(), min_shadow);
  vec3 tmp_color = new_terrain_material_color(clamp(v_splotch, 0.0, 1.0), shadow, make_new_material_params(0.0));
  frag_color = vec4(tmp_color, 1.0);
}