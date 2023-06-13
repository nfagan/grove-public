#version 450

layout (set = 0, binding = 0, std140) uniform UniformData {
  vec4 num_points_xz_t;
  vec4 wind_displacement_info;  //  vec2 wind_displacement_limits, vec2 wind_strength_limits
  vec4 wind_world_bound_xz;
//  Shadow info.
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 shadow_info; //  min_radius_shadow, max_radius_scale_shadow, unused, unused
//  Frag info.
  vec4 sun_position;
  vec4 sun_color;
  vec4 camera_position;
  vec4 color;
  #pragma include "shadow/sample-struct-fields.glsl"
};

struct VS_OUT {
  vec3 normal;
  vec3 light_space_position0;
  vec3 shadow_position;
};

layout (location = 0) in VS_OUT vs_in;
layout (location = 0) out vec4 frag_color;

#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

void main() {
  vec3 sun_light = calculate_sun_light(
    normalize(vs_in.normal), normalize(sun_position.xyz), sun_color.xyz);

  const float shadow = 0.5;
  vec3 light_amount = apply_sun_light_shadow(sun_light, shadow);

  vec3 base_color = srgb_to_linear(color.xyz) * light_amount;
  frag_color = vec4(base_color, 1.0);
}
