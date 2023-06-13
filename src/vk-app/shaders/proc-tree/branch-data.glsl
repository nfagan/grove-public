layout (set = UNIFORM_SET, binding = UNIFORM_BINDING) uniform UniformData {
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;
  vec4 num_points_xz_t; //  vec2(x, y), t, unused
  //  Wind info.
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_info;  //  wind_displacement_limits, wind_strength_limits
  //  Shadow info.
  vec4 shadow_info; //  min_radius_shadow, max_radius_scale_shadow, unused, unused
  //  Frag info.
  vec4 sun_position;
  vec4 sun_color;
  vec4 camera_position;
  vec4 color;
};

struct VS_OUT {
  vec3 normal;
  vec3 light_space_position0;
  vec3 shadow_position;
  float swell_frac;
};