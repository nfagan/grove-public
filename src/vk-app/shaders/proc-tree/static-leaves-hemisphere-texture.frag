#version 450

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;

#pragma include "proc-tree/static-leaves-data.glsl"

layout (set = 1, binding = 0) uniform sampler2D alpha_texture;
layout (set = 1, binding = 1) uniform sampler2D color_texture;

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_shadow_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec4 sampled = texture(alpha_texture, vs_in.v_uv);
  if (sampled.a < 0.5 || length(vs_in.v_uv * 2.0 - 1.0) > 1.0) {
    discard;
  }

  vec3 light = calculate_sun_light(normalize(vs_in.v_normal), normalize(sun_position.xyz), sun_color.rgb);
  float shadow = compute_shadow();
  light = apply_sun_light_shadow(light, shadow);

  sampled.rgb = texture(color_texture, vs_in.v_hemisphere_uv).rgb;
  sampled.rgb *= light;
  frag_color = vec4(sampled.rgb, 1.0);
}
