#version 450

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec3 v_color;
layout (location = 1) in vec3 v_position;
layout (location = 2) in vec3 v_normal;
layout (location = 3) in vec3 v_light_space_position0;
layout (location = 4) flat in float v_rand;

#pragma include "arch/experiment-data.glsl"

#pragma include "shadow/data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (1)
#pragma include "shadow/uniform-buffer.glsl"

layout (set = 0, binding = 2) uniform sampler2DArray sun_shadow_texture;

#pragma include "shadow/sample.glsl"

#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    v_light_space_position0, v_position, camera_position_randomized_color.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 use_color = v_color;
  bool randomized_color = camera_position_randomized_color.w == 1.0;
  if (randomized_color) {
    use_color = vec3(v_rand);
  }

  vec3 light = calculate_sun_light(normalize(v_normal), normalize(sun_position.xyz), sun_color.rgb);
  float shadow = compute_shadow();
  light = apply_sun_light_shadow(light, shadow);
  use_color *= light;

  frag_color = vec4(use_color, 1.0);
}
