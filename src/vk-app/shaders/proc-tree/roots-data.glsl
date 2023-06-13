layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  vec4 num_points_xz_sun_position_xy;
  vec4 sun_position_z_sun_color_xyz;
};

struct VS_OUT {
  vec3 normal;
};
