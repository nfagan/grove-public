struct OrnamentalFoliageSmallInstanceData {
  vec4 translation_direction_x;
  vec4 direction_yz_unused;

  float min_radius_or_aspect;
  float radius_or_scale;
  float radius_power_or_y_rotation_theta;
  float curl_scale;
  float tip_y_fraction;
  float world_origin_x;
  float world_origin_z;

  uint texture_layer_index;
  uint color0;
  uint color1;
  uint color2;
  uint color3;
};