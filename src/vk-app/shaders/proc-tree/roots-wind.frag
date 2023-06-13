#version 450

layout (location = 0) out vec4 frag_color;

#pragma include "proc-tree/roots-wind-data.glsl"
#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"
#pragma include "pack/1u32_to_4fn.glsl"

layout (location = 0) in VS_OUT vs_in;

vec3 get_sun_position() {
  return vec3(uintBitsToFloat(num_points_xz_color_sun_position_xy.zw), sun_position_z_sun_color_xyz.x);
}

vec3 get_sun_color() {
  return sun_position_z_sun_color_xyz.yzw;
}

vec3 get_lin_color() {
#if 0
  return srgb_to_linear(vec3(0.47, 0.26, 0.02));
#else
  return pack_1u32_4fn(num_points_xz_color_sun_position_xy.y).xyz;
#endif
}

void main() {
  vec3 sun_light = calculate_sun_light(normalize(vs_in.normal), normalize(get_sun_position()), get_sun_color());
  vec3 light_amount = apply_sun_light_shadow(sun_light, 1.0);
  vec3 color = get_lin_color();
  frag_color = vec4(color * light_amount, 1.0);
}
