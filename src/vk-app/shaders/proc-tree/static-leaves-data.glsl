struct VS_OUT {
  vec2 v_uv;
  vec3 v_light_space_position0;
  vec3 v_shadow_position;
  vec3 v_normal;
#ifdef INCLUDE_HEMISPHERE_UV
  vec2 v_hemisphere_uv;
#endif
};

layout (std140, set = 0, binding = 0) uniform GlobalUniformData {
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;

  vec4 camera_right_xz;
  vec4 camera_position;

  //  Wind info.
  vec4 wind_world_bound_xz;
  vec4 wind_displacement_limits_wind_strength_limits;
  vec4 time_info; //  t, unused ...

  vec4 sun_position;
  vec4 sun_color;
};