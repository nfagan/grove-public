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

struct VS_OUT {
  vec2 v_uv;
  vec3 v_light_space_position0;
  vec3 v_world_position;
  vec3 v_normal;
#ifdef INCLUDE_INSTANCE_TRANSLATION
  vec3 v_instance_translation;
#endif
};
