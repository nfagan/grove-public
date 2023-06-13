#version 450

#define INSTANCE_UNIFORM_SET (1)
#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)

#define SHADOW_UNIFORM_BUFFER_SET GLOBAL_UNIFORM_SET
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

#pragma include "proc-flower/static-ornament-data.glsl"

layout (set = GLOBAL_UNIFORM_SET, binding = 3) uniform sampler2DArray sun_shadow_texture;
layout (set = INSTANCE_UNIFORM_SET, binding = 0) uniform sampler2D color_texture;

layout (location = 0) out vec4 frag_color;
layout (location = 0) in VS_OUT vs_in;

#pragma include "toon-sun-light.glsl"

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_world_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec4 sampled = texture(color_texture, vs_in.v_uv);
#if 1
  if (sampled.a < 0.5 || length(vs_in.v_uv * 2.0 - 1.0) > 1.0) {
    discard;
  }
#else
  if (sampled.a < 0.5) {
    discard;
  }
#endif
  vec3 light = calculate_sun_light(normalize(vs_in.v_normal), normalize(sun_position.xyz), sun_color.rgb);
  float shadow = compute_shadow();
  light = apply_sun_light_shadow(light, shadow);
  sampled.rgb *= light;
  frag_color = vec4(sampled.rgb, 1.0);
}
