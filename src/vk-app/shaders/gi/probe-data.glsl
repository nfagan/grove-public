struct ProbeTextureInfo {
  int per_probe_inner_texture_dim;
  int per_probe_with_border_texture_dim;
  int per_probe_with_pad_texture_dim;
  int probe_array_texture_dim;
  int probes_per_array_texture_dim;
};

struct ProbeSampleParams {
  bool visibility_test_enabled;
  float normal_bias;
  vec3 camera_position;
};

struct ProbeGridInfo {
  vec3 probe_counts;
  vec3 probe_grid_origin;
  vec3 probe_grid_cell_size;
};

ProbeTextureInfo decode_probe_texture_info(ivec4 info0, ivec4 info1) {
  //  @NOTE see ddgi_common.hpp
  ProbeTextureInfo result;
  result.per_probe_inner_texture_dim = info0[0];
  result.per_probe_with_border_texture_dim = info0[1];
  result.per_probe_with_pad_texture_dim = info0[2];
  result.probe_array_texture_dim = info0[3];
  result.probes_per_array_texture_dim = info1[0];
  return result;
}