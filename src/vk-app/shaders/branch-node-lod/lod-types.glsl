struct RenderBranchNodeLODData {
  uint cull_group_and_instance;  //  1 based cull group, 0 means disabled
  uint is_active;
  uint unused_reserved2;
  uint unused_reserved3;
};

struct LODOutputData {
  uint lod_index;
  uint unused_reserved1;
  uint unused_reserved2;
  uint unused_reserved3;
};