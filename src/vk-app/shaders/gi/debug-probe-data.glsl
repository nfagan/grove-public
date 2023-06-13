layout (set = 0, binding = 1) uniform UniformData {
  vec4 probe_counts;
  vec4 probe_grid_origin;
  vec4 probe_grid_cell_size;
  ivec4 irr_texture_info0;
  ivec4 irr_texture_info1;
  vec4 sample_params0;  //  irradiance scale, check finite, unused ...
  mat4 projection_view;
};