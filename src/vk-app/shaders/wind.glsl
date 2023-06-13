vec2 sample_wind_tip_displacement(vec2 world_pos_xz, vec4 wind_world_bound_xz, sampler2D wind_displacement_texture) {
  vec2 world_p0 = wind_world_bound_xz.xy;
  vec2 world_p1 = wind_world_bound_xz.zw;
  vec2 p = (world_pos_xz - world_p0) / (world_p1 - world_p0);
  return texture(wind_displacement_texture, p).rg;
}

float tip_wind_displacement_fraction(float u) {
  return (1.0 / 3.0) * pow(u, 4.0) - (4.0 / 3.0) * pow(u, 3.0) + 2.0 * pow(u, 2.0);
}

vec2 wind_oscillation(vec2 wind_direction, float elapsed_time, float rand01) {
  return wind_direction * cos(elapsed_time * (3.0 + rand01 * 1.5));
}