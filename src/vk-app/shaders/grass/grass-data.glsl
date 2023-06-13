struct VS_OUT {
  float v_y;
  float v_alpha;
  vec3 v_position;
  vec3 light_space_position0;
  vec2 color_uv;
};

layout (std140, set = GRASS_UNIFORM_SET, binding = GRASS_UNIFORM_BINDING) uniform GrassUniformData {
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;
  vec4 camera_position;

  vec4 camera_front_xz;
  vec4 blade_scale_taper_power;
  vec4 next_blade_scale;

  vec4 frustum_grid_dims; //  vec2 cell size, float offset, float extent
  vec4 extent_info; // vec2 z-extent, float scale0, float scale1

  vec4 sun_position;
  vec4 sun_color;

  vec4 wind_world_bound_xz;
  vec4 time_info;
  vec4 terrain_grid_scale_max_diffuse_max_specular;  //  grid scale, max diff, max spec, unused
  vec4 min_shadow_global_color_scale_discard_at_edge; //  min, max, discard at edge ...
};