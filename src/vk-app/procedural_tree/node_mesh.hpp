#pragma once

#include "components.hpp"

namespace grove::tree {

struct MakeNodeMeshParams {
  bool include_uv{true};
  bool allow_branch_to_lateral_child{true};
  float leaf_tip_radius{};
  float scale{1.0f};
  Vec3f offset{};
};

size_t compute_num_vertices_in_node_mesh(const Vec2<int>& geom_sizes_xz, uint32_t num_internodes);
size_t compute_num_indices_in_node_mesh(const Vec2<int>& geom_sizes_xz, uint32_t num_internodes);
void make_node_mesh(const tree::Internode* internodes, uint32_t num_internodes,
                    const Vec2<int>& geom_sizes_xz, const MakeNodeMeshParams& params,
                    float* out_v, uint16_t* out_i);

struct AmplifyGeometryOrientedAtInternodesParams {
  const Vec3f* positions;
  const Vec3f* directions;
  uint32_t num_elements;

  const void* src;
  uint32_t src_byte_stride;
  uint32_t src_position_byte_offset;
  Optional<uint32_t> src_normal_byte_offset;
  Optional<uint32_t> src_uv_byte_offset;
  uint32_t num_src_vertices;

  void* dst;
  uint32_t dst_byte_stride;
  uint32_t dst_position_byte_offset;
  uint32_t dst_normal_byte_offset;  //  must be present if present and absent if absent in src
  uint32_t dst_uv_byte_offset;      //  must be present if present and absent if absent in src
  uint32_t max_num_dst_vertices;

  float scale;
};

void amplify_geometry_oriented_at_internodes(const AmplifyGeometryOrientedAtInternodesParams& params);

}