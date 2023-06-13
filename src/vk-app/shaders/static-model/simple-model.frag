#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UNIFORM_SET (1)
#define UNIFORM_BINDING (0)
#pragma include "static-model/simple-model-data.glsl"

#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

#define NUM_SHADOW_SAMPLES (4)

#pragma include "shadow/data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (0)
#pragma include "shadow/uniform-buffer.glsl"

layout (location = 0) out vec4 frag_color;
layout (location = 0) in VS_OUT vs_in;

layout (set = 0, binding = 1) uniform sampler2DArray sun_shadow_texture;
layout (set = 1, binding = 1) uniform sampler2D color_texture;

#pragma include "shadow/sample.glsl"
#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.light_space_position0, vs_in.position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 color = texture(color_texture, vs_in.uv).rgb;
  vec3 light = calculate_sun_light(normalize(vs_in.normal), normalize(sun_position.xyz), sun_color.rgb);
  float shadow = compute_shadow();
  light = apply_sun_light_shadow(light, shadow);
  color *= light;

  frag_color = vec4(color, 1.0);
}
