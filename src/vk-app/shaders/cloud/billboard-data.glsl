layout (std140, push_constant) uniform PushConstantData {
  vec4 uvw_offset;
  vec4 scale_depth_test_enable;
  vec4 translation_opacity_scale;
  vec4 camera_right_front;
  mat4 projection_view;
};