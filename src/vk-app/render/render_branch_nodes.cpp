#include "render_branch_nodes.hpp"
#include "render_branch_nodes_types.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/ArrayView.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

void set_position_and_radii(RenderBranchNodeDynamicData& node,
                            const RenderBranchNodeInstanceDescriptor& desc) {
  node.set_position_and_radii(
    desc.self_position, desc.self_radius,
    desc.child_position, desc.child_radius);
}

void set_directions(RenderBranchNodeStaticData& data, const RenderBranchNodeInstanceDescriptor& desc) {
  data.set_directions(desc.self_right, desc.self_up, desc.child_right, desc.child_up);
}

RenderBranchNodeStaticData to_static_data(const RenderBranchNodeInstanceDescriptor& desc,
                                          uint32_t aggregate_index) {
  RenderBranchNodeStaticData result{};
  set_directions(result, desc);
  result.aggregate_index_unused = Vec4<uint32_t>{aggregate_index, 0, 0, 0};
  return result;
}

RenderWindBranchNodeStaticData to_wind_static_data(const RenderBranchNodeInstanceDescriptor& desc,
                                                   uint32_t aggregate_index) {
  RenderWindBranchNodeStaticData result;
  result.base = to_static_data(desc, aggregate_index);
  result.wind_info0 = desc.wind_info0;
  result.wind_info1 = desc.wind_info1;
  result.wind_info2 = desc.wind_info2;
  return result;
}

RenderBranchNodeDynamicData to_dynamic_data(const RenderBranchNodeInstanceDescriptor& desc) {
  RenderBranchNodeDynamicData result{};
  set_position_and_radii(result, desc);
  return result;
}

RenderBranchNodeAggregate to_aggregate(const RenderBranchNodeAggregateDescriptor& desc) {
  RenderBranchNodeAggregate result{};
  result.aabb_p0_unused = Vec4f{desc.aabb_p0, 0.0f};
  result.aabb_p1_unused = Vec4f{desc.aabb_p1, 0.0f};
  return result;
}

template <typename InstanceSet>
uint32_t reserve(InstanceSet& dst_set, uint32_t num_instances,
                 ContiguousElementGroupAllocator::ElementGroupHandle* gh) {
  const auto curr_num_insts = uint32_t(dst_set.static_instances.size());
  assert(curr_num_insts == uint32_t(dst_set.dynamic_instances.size()));

  (void) dst_set.alloc.reserve(num_instances, gh);

  const auto new_num_insts = curr_num_insts + num_instances;
  dst_set.static_instances.resize(new_num_insts);
  dst_set.dynamic_instances.resize(new_num_insts);
  dst_set.lod_data.resize(new_num_insts);

  while (gh->index >= dst_set.aggregates.size()) {
    dst_set.aggregates.emplace_back();
  }

  dst_set.static_instances_modified = true;
  dst_set.dynamic_instances_modified = true;
  dst_set.lod_data_modified = true;
  dst_set.aggregates_modified = true;

  return curr_num_insts;
}

template <typename InstanceSet>
void release(InstanceSet& dst_set, uint32_t group_index,
             ContiguousElementGroupAllocator::Movement* move, size_t static_size, size_t dyn_size) {
  assert(dst_set.static_instances.size() == dst_set.dynamic_instances.size());

  dst_set.alloc.release(ContiguousElementGroupAllocator::ElementGroupHandle{group_index});
  uint32_t new_num_insts{};
  (void) dst_set.alloc.arrange_implicit(move, &new_num_insts);

  move->apply(dst_set.static_instances.data(), static_size);
  move->apply(dst_set.dynamic_instances.data(), dyn_size);
  move->apply(dst_set.lod_data.data(), sizeof(RenderBranchNodeLODData));

  dst_set.static_instances.resize(new_num_insts);
  dst_set.dynamic_instances.resize(new_num_insts);
  dst_set.lod_data.resize(new_num_insts);

  dst_set.static_instances_modified = true;
  dst_set.dynamic_instances_modified = true;
  dst_set.lod_data_modified = true;
  dst_set.aggregates_modified = true;
}

template <typename InstanceSet>
ArrayView<RenderBranchNodeDynamicData> get_dynamic_data(InstanceSet& dst_set, uint32_t gi) {
  assert(gi < dst_set.aggregates.size());
  auto* group = dst_set.alloc.read_group(ContiguousElementGroupAllocator::ElementGroupHandle{gi});
  return ArrayView<RenderBranchNodeDynamicData>{
    dst_set.dynamic_instances.data() + group->offset,
    dst_set.dynamic_instances.data() + group->offset + group->count,
  };
}

ArrayView<RenderBranchNodeStaticData> get_static_data(RenderBranchNodesData::BaseSet& dst_set,
                                                      uint32_t gi) {
  assert(gi < dst_set.aggregates.size());
  auto* group = dst_set.alloc.read_group(ContiguousElementGroupAllocator::ElementGroupHandle{gi});
  return ArrayView<RenderBranchNodeStaticData>{
    dst_set.static_instances.data() + group->offset,
    dst_set.static_instances.data() + group->offset + group->count,
  };
}

template <typename InstanceSet>
ArrayView<RenderBranchNodeLODData> get_lod_data(InstanceSet& dst_set, uint32_t gi) {
  assert(gi < dst_set.aggregates.size());
  auto* group = dst_set.alloc.read_group(ContiguousElementGroupAllocator::ElementGroupHandle{gi});
  return ArrayView<RenderBranchNodeLODData>{
    dst_set.lod_data.data() + group->offset,
    dst_set.lod_data.data() + group->offset + group->count,
  };
}

struct {
  RenderBranchNodesData render_branch_nodes_data;
} globals;

} //  anon

RenderBranchNodesData* tree::get_global_branch_nodes_data() {
  return &globals.render_branch_nodes_data;
}

WindBranchNodeDrawableHandle
tree::create_wind_branch_node_drawable(RenderBranchNodesData* rd,
                                       const RenderBranchNodeInstanceDescriptor* instances,
                                       uint32_t num_instances,
                                       const RenderBranchNodeAggregateDescriptor& aggregate) {
  auto& dst_set = rd->wind_set;

  ContiguousElementGroupAllocator::ElementGroupHandle gh{};
  const auto curr_num_insts = reserve(dst_set, num_instances, &gh);

  const uint32_t aggregate_index = gh.index;
  dst_set.aggregates[aggregate_index] = to_aggregate(aggregate);

  for (uint32_t i = 0; i < num_instances; i++) {
    const uint32_t ind = i + curr_num_insts;
    dst_set.static_instances[ind] = to_wind_static_data(instances[i], aggregate_index);
    dst_set.dynamic_instances[ind] = to_dynamic_data(instances[i]);
  }

  WindBranchNodeDrawableHandle result{};
  result.id = gh.index;
  return result;
}

void tree::destroy_wind_branch_node_drawable(RenderBranchNodesData* rd,
                                             WindBranchNodeDrawableHandle handle) {
  auto& dst_set = rd->wind_set;
  ContiguousElementGroupAllocator::Movement move{};
  release(
    dst_set, handle.id, &move,
    sizeof(RenderWindBranchNodeStaticData), sizeof(RenderBranchNodeDynamicData));
  (void) move;
}

BranchNodeDrawableHandle
tree::create_branch_node_drawable(RenderBranchNodesData* rd,
                                  const RenderBranchNodeInstanceDescriptor* instances,
                                  uint32_t num_instances,
                                  const RenderBranchNodeAggregateDescriptor& aggregate) {
  auto& dst_set = rd->base_set;

  ContiguousElementGroupAllocator::ElementGroupHandle gh{};
  const auto curr_num_insts = reserve(dst_set, num_instances, &gh);

  const uint32_t aggregate_index = gh.index;
  dst_set.aggregates[aggregate_index] = to_aggregate(aggregate);

  for (uint32_t i = 0; i < num_instances; i++) {
    const uint32_t ind = i + curr_num_insts;
    dst_set.static_instances[ind] = to_static_data(instances[i], aggregate_index);
    dst_set.dynamic_instances[ind] = to_dynamic_data(instances[i]);
  }

  BranchNodeDrawableHandle result{};
  result.id = gh.index;
  return result;
}

void tree::destroy_branch_node_drawable(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle) {
  auto& dst_set = rd->base_set;
  ContiguousElementGroupAllocator::Movement move{};
  release(
    dst_set, handle.id, &move,
    sizeof(RenderBranchNodeStaticData), sizeof(RenderBranchNodeDynamicData));
  (void) move;
}

ArrayView<RenderBranchNodeDynamicData>
tree::get_branch_nodes_dynamic_data(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle handle) {
  return get_dynamic_data(rd->wind_set, handle.id);
}

void tree::set_branch_nodes_dynamic_data_modified(RenderBranchNodesData* rd,
                                                  WindBranchNodeDrawableHandle) {
  rd->wind_set.dynamic_instances_modified = true;
}

ArrayView<RenderBranchNodeDynamicData>
tree::get_branch_nodes_dynamic_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle) {
  return get_dynamic_data(rd->base_set, handle.id);
}

void tree::set_branch_nodes_dynamic_data_modified(RenderBranchNodesData* rd,
                                                  BranchNodeDrawableHandle) {
  rd->base_set.dynamic_instances_modified = true;
}

ArrayView<RenderBranchNodeStaticData>
tree::get_branch_nodes_static_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle) {
  return get_static_data(rd->base_set, handle.id);
}

void tree::set_branch_nodes_static_data_modified(RenderBranchNodesData* rd, BranchNodeDrawableHandle) {
  rd->base_set.static_instances_modified = true;
}

ArrayView<RenderBranchNodeLODData>
tree::get_branch_nodes_lod_data(RenderBranchNodesData* rd, BranchNodeDrawableHandle handle) {
  return get_lod_data(rd->base_set, handle.id);
}

void tree::set_branch_nodes_lod_data_modified(RenderBranchNodesData* rd, BranchNodeDrawableHandle) {
  rd->base_set.lod_data_modified = true;
}

void tree::set_branch_nodes_lod_data_potentially_invalidated(RenderBranchNodesData* rd) {
  assert(rd->base_set.lod_data_modified);
  rd->base_set.lod_data_potentially_invalidated = true;
}

ArrayView<RenderBranchNodeLODData>
tree::get_branch_nodes_lod_data(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle handle) {
  return get_lod_data(rd->wind_set, handle.id);
}

void tree::set_branch_nodes_lod_data_modified(RenderBranchNodesData* rd, WindBranchNodeDrawableHandle) {
  rd->wind_set.lod_data_modified = true;
}

GROVE_NAMESPACE_END
