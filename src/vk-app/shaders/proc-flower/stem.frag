#version 450

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#pragma include "proc-flower/stem-data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET GLOBAL_UNIFORM_SET
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

layout (set = GLOBAL_UNIFORM_SET, binding = 3) uniform sampler2DArray sun_shadow_texture;

#define NO_PCF

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_world_position;
layout (location = 1) in vec3 v_light_space_position0;

#pragma include "color/srgb-to-linear.glsl"
#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_light_space_position0, v_world_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}


void main() {
  vec3 color = srgb_to_linear(color_wind_influence_enabled.xyz);
  color *= apply_sun_light_shadow(sun_color.rgb, compute_shadow());
  frag_color = vec4(color, 1.0);
}
