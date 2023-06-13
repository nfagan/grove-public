#version 450

#define NO_PCF

#define GRASS_UNIFORM_SET (0)
#define GRASS_UNIFORM_BINDING (0)
#pragma include "grass/grass-data.glsl"

#define NUM_SHADOW_SAMPLES (4)
#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

#define NEW_MAT_BINDING (5)

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;

layout (location = 0) out vec4 frag_color;
layout (location = 0) in VS_OUT vs_out;

const vec3 up = vec3(0.0, 1.0, 0.0);

#pragma include "grass/new-material.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_out.light_space_position0, vs_out.v_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  bool discard_at_edge = min_shadow_global_color_scale_discard_at_edge.z > 0.0;
  if (discard_at_edge && vs_out.v_alpha < 0.99999) {
    discard;
  } else if (!discard_at_edge && vs_out.v_alpha >= 0.99999) {
    discard;
  }

  float y = max(0.0, vs_out.v_y);
  float splotch = vs_out.color_uv.x;
  float min_shadow = min_shadow_global_color_scale_discard_at_edge.x;
  float shadow = max(compute_shadow(), min_shadow);
  vec3 v = normalize(camera_position.xyz - vs_out.v_position);

  vec3 tmp_color = new_material_color(y, splotch, v, sun_position.xyz, sun_color.xyz, shadow, make_new_material_params(1.0));
  frag_color = vec4(tmp_color, vs_out.v_alpha);
}