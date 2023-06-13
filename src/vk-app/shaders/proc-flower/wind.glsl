//  first include "wind.glsl"
vec2 wind_displacement(float t, float y_frac, vec2 sampled_wind, vec2 world_xz) {
  float phase = length(world_xz);
  return sampled_wind * tip_wind_displacement_fraction(y_frac) * sin(t * 5.0 + phase) * 0.1;
}