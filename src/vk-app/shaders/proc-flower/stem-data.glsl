layout (std140, set = GLOBAL_UNIFORM_SET, binding = GLOBAL_UNIFORM_BINDING) uniform GlobalUniformData {
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;
  vec4 num_points_xz_t; //  x, z, t, unused
  vec4 wind_world_bound_xz;
  vec4 camera_position;
  vec4 sun_color;
};

layout (push_constant) uniform PushConstantData {
  vec4 color_wind_influence_enabled;
};
