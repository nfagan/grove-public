struct ShapeParams {
  float min_radius;
  float radius;
  float radius_power;
  float mix_texture_color;

  float circumference_frac0;
  float circumference_frac1;
  float circumference_frac_power;
  float curl_scale;

  vec2 scale;
  float min_z_discard_enabled;
  float group_frac;
};

struct VS_OUT {
  vec2 v_normalized_position;
  vec2 v_uv;
  float v_min_z_discard_enabled;
  float v_mix_texture_color;
  float v_displace_t;
  vec3 v_world_position;
  vec3 v_light_space_position0;
};

layout (std140, set = GLOBAL_UNIFORM_SET, binding = GLOBAL_UNIFORM_BINDING) uniform GlobalUniformData {
  mat4 projection;
  mat4 view;
  mat4 sun_light_view_projection0;
  vec4 wind_world_bound_xz;
  vec4 time_info; //  t, unused...
  vec4 camera_position;
  vec4 sun_position;
  vec4 sun_color;
};

layout (std140, set = INSTANCE_UNIFORM_SET, binding = INSTANCE_UNIFORM_BINDING) uniform InstanceUniformData {
  vec4 num_grid_points_xz;  //  x, z, unused ...
  vec4 group_scale;
  vec4 color_info0;
  ivec4 component_indices_alpha_test_enabled;
};