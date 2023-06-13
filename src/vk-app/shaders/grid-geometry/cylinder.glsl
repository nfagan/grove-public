vec3 cylinder_shape_function(vec2 num_points_xz, vec2 position, float pi) {
  float x_dim = floor(num_points_xz.x * 0.5);
  float x_ind = position.x == x_dim ? 0.0 : position.x + x_dim;
  float theta = (2.0 * pi) * (x_ind / (num_points_xz.x - 1.0));
  float y = position.y / (num_points_xz.y - 1.0);
  return vec3(cos(theta), y, sin(theta));
}