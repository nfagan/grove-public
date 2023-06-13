struct PetalParameters {
  float scale_x;
  float scale_y;
  float radius;
  float radius_power;
  float min_radius;
  float max_additional_radius;
  float max_negative_y_offset;
  float circum_frac0;
  float circum_frac1;
  float circum_frac_power;
  float wind_fast_osc_amplitude;
  float pad1;
};

struct BlowingPetalParameters {
  vec4 time_info; //  t, duration, unused ...
  vec4 scale;
};

struct PetalTransformData {
  vec4 translation;
};