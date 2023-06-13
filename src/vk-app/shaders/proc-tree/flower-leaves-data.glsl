struct VS_OUT {
  vec3 v_light_space_position0;
  vec3 v_shadow_position;
  vec2 v_uv;
  vec2 v_relative_position;
  float v_animation_t;
  float v_petal_rand;
#ifdef USE_HEMISPHERE_COLOR_IMAGE
  vec2 v_hemisphere_uv;
#endif
};

layout (std140, set = GLOBAL_UNIFORM_SET, binding = GLOBAL_UNIFORM_BINDING) uniform GlobalUniformData {
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

layout (std140, set = INSTANCE_UNIFORM_SET, binding = INSTANCE_UNIFORM_BINDING) uniform InstanceUniformData {
  PetalParameters petal_params;
  BlowingPetalParameters blowing_petal_params;

  vec4 aabb_p0_padded;
  vec4 aabb_p1_padded;
  vec4 model_scale_shadow_scale;

  vec4 color0;
  vec4 color1;
  vec4 color2;

  vec4 num_points;  //  x, z, unused ...
};