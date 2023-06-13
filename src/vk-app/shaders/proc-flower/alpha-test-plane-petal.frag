#version 450

#define DO_ALPHA_TEST (1)

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)

#define INSTANCE_UNIFORM_SET (1)
#define INSTANCE_UNIFORM_BINDING (0)

#pragma include "proc-flower/petal-color.glsl"
#pragma include "proc-flower/alpha-test-data.glsl"

#define SHADOW_UNIFORM_BUFFER_SET GLOBAL_UNIFORM_SET
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/data.glsl"
#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/sample.glsl"

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

layout (set = INSTANCE_UNIFORM_SET, binding = 1) uniform sampler2D alpha_texture;
layout (set = GLOBAL_UNIFORM_SET, binding = 3) uniform sampler2DArray sun_shadow_texture;

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_world_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  ivec3 component_indices = component_indices_alpha_test_enabled.xyz;
  int alpha_test_enabled = component_indices_alpha_test_enabled.w;

  vec2 uv = vs_in.v_uv;
  vec4 tex_color = texture(alpha_texture, uv);
  float alpha = tex_color.a;
#if DO_ALPHA_TEST
  if (alpha_test_enabled == 1 && alpha < 0.005 && vs_in.v_normalized_position.y > vs_in.v_min_z_discard_enabled) {
    discard;
  }
#endif
  float x01 = vs_in.v_normalized_position.x;
  float z01 = vs_in.v_normalized_position.y;
  float displace_alpha = 1.0 - pow(vs_in.v_displace_t, 8.0);
  vec3 color = compute_color(x01, z01, color_info0, component_indices);
  color = mix(color, tex_color.rgb, vs_in.v_mix_texture_color);
  color *= compute_shadow();
  frag_color = vec4(color, displace_alpha);
}