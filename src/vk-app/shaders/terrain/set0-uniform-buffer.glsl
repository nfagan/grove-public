#ifndef NUM_SUN_SHADOW_CASCADES
#error "Expected NUM_SUN_SHADOW_CASCADES define"
#endif

layout (std140, set = 0, binding = 0) uniform UniformData {
  vec4 shadow_cascade_extents;
  vec4 shadow_cascade_uv_scales[NUM_SUN_SHADOW_CASCADES];
  vec4 shadow_cascade_uv_offsets[NUM_SUN_SHADOW_CASCADES];
  mat4 light_view_projection0;
  mat4 view;
  vec4 camera_position;
  vec4 sun_pos_color_r;
  vec4 sun_color_gb_time;
  vec4 wind_world_bound_xz;
  vec4 min_shadow_global_color_scale_unused;
};