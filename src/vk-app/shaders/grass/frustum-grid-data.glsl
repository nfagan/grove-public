struct FrustumGridInfo {
  vec4 info;
};

layout (std140, set = FRUSTUM_GRID_SET, binding = FRUSTUM_GRID_BINDING) readonly buffer FrustumGridData {
  FrustumGridInfo frustum_grid_info[];
};