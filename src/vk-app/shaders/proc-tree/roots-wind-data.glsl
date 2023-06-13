layout (std140, push_constant) uniform PushConstantData {
  mat4 projection_view;
  uvec4 num_points_xz_color_sun_position_xy;
  vec4 sun_position_z_sun_color_xyz;
  vec4 aabb_p0_t;
  vec4 aabb_p1_wind_strength;
};

struct VS_OUT {
  vec3 normal;
};