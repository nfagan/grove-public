#version 450
#extension GL_ARB_separate_shader_objects : enable

#define GLOBAL_UNIFORM_SET (0)
#define GLOBAL_UNIFORM_BINDING (0)
#define INSTANCE_UNIFORM_SET (1)
#define INSTANCE_UNIFORM_BINDING (0)

#pragma include "proc-tree/flower-leaves-data-common.glsl"
#pragma include "proc-tree/flower-leaves-data.glsl"

#ifdef USE_HEMISPHERE_COLOR_IMAGE
layout (set = INSTANCE_UNIFORM_SET, binding = 2) uniform sampler2D color_image;
#endif

#define SHADOW_UNIFORM_BUFFER_SET (0)
#define SHADOW_UNIFORM_BUFFER_BINDING (2)

#pragma include "shadow/uniform-buffer.glsl"
#pragma include "shadow/data.glsl"
#pragma include "shadow/sample.glsl"

layout (set = 0, binding = 3) uniform sampler2DArray sun_shadow_texture;

#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

#ifndef USE_HEMISPHERE_COLOR_IMAGE
vec3 gradient1_color(float x11, float z01) {
  float grad_pow = pow(1.0 - z01, 2.0);
  vec3 color = mix(color0.xyz, color1.xyz, grad_pow);

  float abs_x = abs(x11);
  vec3 highlight = mix(vec3(0.0), color2.xyz, 1.0 - abs_x);
  color += highlight * grad_pow;

  return color;
}
#endif

float compute_shadow() {
  vec3 shadow_uvw;
  float shadow = simple_sample_shadow(
    vs_in.v_light_space_position0, vs_in.v_shadow_position, camera_position.xyz, view, sun_shadow_texture, shadow_uvw);
  return shadow;
}

void main() {
  vec3 sun_light = calculate_sun_light(vec3(0.0, 1.0, 0.0), normalize(sun_position.xyz), sun_color.xyz);
  float shadow = compute_shadow();

  float anim_t = clamp(vs_in.v_animation_t * (2.0 + vs_in.v_petal_rand), 0.0, 1.0);
  float alpha = 1.0 - pow(anim_t, 4.0);

#ifdef USE_HEMISPHERE_COLOR_IMAGE
  vec3 color = texture(color_image, vs_in.v_hemisphere_uv).rgb;
#else
  vec3 color = gradient1_color(vs_in.v_relative_position.x, vs_in.v_relative_position.y);
  color = srgb_to_linear(color);
#endif
  vec3 light_amount = apply_sun_light_shadow(sun_light, shadow);
  frag_color = vec4(color * light_amount, alpha);
}