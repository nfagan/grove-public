#version 450

layout (location = 0) out vec4 frag_color;

#pragma include "proc-tree/roots-data.glsl"
#pragma include "toon-sun-light.glsl"
#pragma include "color/srgb-to-linear.glsl"

layout (location = 0) in VS_OUT vs_in;

vec3 get_sun_position() {
  return vec3(num_points_xz_sun_position_xy.zw, sun_position_z_sun_color_xyz.x);
}

vec3 get_sun_color() {
  return sun_position_z_sun_color_xyz.yzw;
}

void main() {
  vec3 sun_light = calculate_sun_light(normalize(vs_in.normal), normalize(get_sun_position()), get_sun_color());
  vec3 light_amount = apply_sun_light_shadow(sun_light, 1.0);
  vec3 color = srgb_to_linear(vec3(0.47, 0.26, 0.02));
  frag_color = vec4(color * light_amount, 1.0);
}
