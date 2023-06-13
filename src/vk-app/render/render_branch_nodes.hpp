#pragma once

#include "grove/math/vector.hpp"

namespace grove {
template <typename T>
class ArrayView;
}

namespace grove::tree {

struct RenderBranchNodesData;
struct RenderBranchNodeDynamicData;
struct RenderBranchNodeStaticData;
struct RenderBranchNodeLODData;

struct RenderBranchNodeInstanceDescriptor {
  uint32_t frustum_cull_instance_group; //  @NOTE: use 1 based index, 0 indicates no culling
  uint32_t frustum_cull_instance;
  Vec3f self_position;
  float self_radius;
  Vec3f child_position;
  float child_radius;
  Vec3f self_right;
  Vec3f self_up;
  Vec3f child_right;
  Vec3f child_up;
  Vec4<uint32_t> wind_info0;
  Vec4<uint32_t> wind_info1;
  Vec4<uint32_t> wind_info2;
};

struct RenderBranchNodeAggregateDescriptor {
  Vec3f aabb_p0;
  Vec3f aabb_p1;
};

struct BranchNodeDrawableHandle {
  uint32_t id;
};

struct WindBranchNodeDrawableHandle {
  uint32_t id;
};

RenderBranchNodesData* get_global_branch_nodes_data();

BranchNodeDrawableHandle
create_branch_node_drawable(RenderBranchNodesData* rd,
                            const RenderBranchNodeInstanceDescriptor* instances,
                            uint32_t num_instances,
                            const RenderBranchNodeAggregateDescriptor& aggregate);

WindBranchNodeDrawableHandle
create_wind_branch_node_drawable(RenderBranchNodesData* rd,
                                 const RenderBranchNodeInstanceDescriptor* instances,
                                 uint32_t num_instances,
                                 const RenderBranchNodeAggregateDescriptor& aggregate);

void destroy_branch_node_drawable(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle);
void destroy_wind_branch_node_drawable(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle handle);

ArrayView<RenderBranchNodeDynamicData>
get_branch_nodes_dynamic_data(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle handle);
void set_branch_nodes_dynamic_data_modified(RenderBranchNodesData* rd,
                                            WindBranchNodeDrawableHandle handle);

ArrayView<RenderBranchNodeDynamicData>
get_branch_nodes_dynamic_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle);
void set_branch_nodes_dynamic_data_modified(RenderBranchNodesData* rd,
                                            BranchNodeDrawableHandle handle);

ArrayView<RenderBranchNodeStaticData>
get_branch_nodes_static_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle);
void set_branch_nodes_static_data_modified(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle);

ArrayView<RenderBranchNodeLODData>
get_branch_nodes_lod_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle);
void set_branch_nodes_lod_data_modified(RenderBranchNodesData* rd,
                                        BranchNodeDrawableHandle handle);

void set_branch_nodes_lod_data_potentially_invalidated(RenderBranchNodesData* rd);

ArrayView<RenderBranchNodeLODData>
get_branch_nodes_lod_data(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle handle);
void set_branch_nodes_lod_data_modified(RenderBranchNodesData* rd,
                                        WindBranchNodeDrawableHandle handle);

}