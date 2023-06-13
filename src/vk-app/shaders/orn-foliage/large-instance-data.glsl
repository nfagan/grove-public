struct OrnamentalFoliageLargeInstanceAggregateData {
  vec4 aggregate_aabb_p0;
  vec4 aggregate_aabb_p1;
};

struct OrnamentalFoliageLargeInstanceData {
  vec4 translation_direction_x;
  vec4 direction_yz_unused;
  float min_radius_or_aspect;
  float radius_or_scale;
  float radius_power_or_y_rotation_theta;
  float curl_scale;
  uint texture_layer_index;
  uint color0;
  uint color1;
  uint color2;
  uint color3;
  uint aggregate_index;
  uint pad1;
  uint pad2;
  uvec4 wind_info0;
  uvec4 wind_info1;
  uvec4 wind_info2;
};