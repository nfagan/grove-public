#version 450

#define UNIFORM_SET (0)
#define UNIFORM_BINDING (0)

#pragma include "proc-tree/branch-data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"
#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.light_space_position0, vs_in.shadow_position, camera_position.xyz,
    view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 sun_light = calculate_sun_light(
    normalize(vs_in.normal), normalize(sun_position.xyz), sun_color.xyz);
  float shadow = compute_shadow();
  vec3 light_amount = apply_sun_light_shadow(sun_light, shadow);

#if 1
  vec3 base_color = srgb_to_linear(color.xyz) * light_amount + vs_in.swell_frac * 0.25;
  frag_color = vec4(base_color, 1.0);
#else
//  frag_color = vec4(srgb_to_linear(color.xyz), 1.0);
  frag_color = vec4(pow(color.xyz, vec3(2.2)), 1.0);
#endif
}