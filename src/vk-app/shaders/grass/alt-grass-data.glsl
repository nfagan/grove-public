struct VS_OUT {
  vec2 v_normalized_position;
  float v_rand;
  vec3 v_scale;
  float v_alpha;
  vec3 v_light_space_position0;
  vec3 v_world_pos;
  vec2 v_world_uv;
  float v_sun_fade_out_frac;
};

layout (set = UNIFORM_SET, binding = UNIFORM_BINDING, std140) uniform UniformData {
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;

  vec4 camera_position;
  vec4 frustum_grid_cell_size_terrain_grid_scale;
  vec4 wind_world_bound_xz;

  vec4 near_scale_info; //  extent xy, scale zw
  vec4 far_scale_info;  //  extent xy, scale zw

  vec4 time_max_diffuse_max_specular; //  t, max diffuse, max specular, unused

  vec4 min_shadow_global_color_scale; //  min, max, unused, unused
  vec4 sun_position;
  vec4 sun_color;
};