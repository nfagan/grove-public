layout (set = UNIFORM_DATA_SET, binding = UNIFORM_DATA_BINDING) uniform UniformData {
  mat4 model;
  mat4 view;
  mat4 projection;
  mat4 sun_light_view_projection0;
  vec4 camera_position;
  vec4 min_shadow_global_color_scale; //  min_shadow, global_color_scale, unused ...
};