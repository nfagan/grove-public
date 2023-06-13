struct ComputeLODIndex {
  uint index;
};

struct RenderInstanceComponentIndices {
  uint frustum_cull_group;
  uint frustum_cull_instance_index;
  uint is_active;
  uint occlusion_cull_group_cluster_instance_index;
};