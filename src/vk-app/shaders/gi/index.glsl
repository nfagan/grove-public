ivec3 linear_probe_index_to_grid_index(int linear_ind, ivec3 probe_cts) {
  int z_ind = linear_ind / (probe_cts.x * probe_cts.y);
  int xy_linear_ind = linear_ind - z_ind * probe_cts.x * probe_cts.y;
  int y_ind = xy_linear_ind / probe_cts.x;
  int x_ind = xy_linear_ind - y_ind * probe_cts.x;
  return ivec3(x_ind, y_ind, z_ind);
}

int probe_grid_index_to_linear_index(ivec3 grid_index, ivec3 probe_cts) {
  int slab_ind = grid_index.z * probe_cts.x * probe_cts.y;
  int row_ind = grid_index.y * probe_cts.x;
  int col_ind = grid_index.x;
  return slab_ind + col_ind + row_ind;
}