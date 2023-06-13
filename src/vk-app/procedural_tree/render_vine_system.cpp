#include "render_vine_system.hpp"
#include "../render/render_vines.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ContiguousElementGroupAllocator.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

} //  anon

namespace tree {

struct VineAttachedToAggregateRenderDataSlotState {
  bool in_use;
  VineRenderSegmentHandle segment_handle;
};

struct RenderVineSystem {
  ContiguousElementGroupAllocator segment_alloc;
  std::vector<VineRenderNode> render_nodes;
  std::vector<VineAttachedToAggregateRenderData> aggregate_render_data;
  std::vector<VineAttachedToAggregateRenderDataSlotState> aggregate_slot_state;
  bool render_nodes_modified{};
};

} //  tree

namespace {

uint16_t float_to_u16(float v) {
  v = clamp(v, -1.0f, 1.0f);
  return uint16_t(clamp((v * 0.5f + 0.5f) * float(0xffff), 0.0f, float(0xffff)));
}

uint32_t to_u32(float c, float s) {
  return (uint32_t(float_to_u16(c)) << 16u) | uint32_t(float_to_u16(s));
}

void encode_directions(const Vec3f& self_right, const Vec3f& self_up,
                       const Vec3f& child_right, const Vec3f& child_up,
                       Vec4<uint32_t>* directions0,
                       Vec4<uint32_t>* directions1) {
  Vec4<uint32_t> dir0{};
  for (int i = 0; i < 3; i++) {
    dir0[i] = to_u32(child_right[i], self_right[i]);
  }
  dir0[3] = to_u32(child_up[0], self_up[0]);

  Vec4<uint32_t> dir1{};
  for (int i = 0; i < 2; i++) {
    dir1[i] = to_u32(child_up[i+1], self_up[i+1]);
  }

  *directions0 = dir0;
  *directions1 = dir1;
}

void set_positions(VineRenderNode& node, const Vec3f& self_p, const Vec3f& child_p) {
  float self_r = node.self_position_radius.w;
  float child_r = node.child_position_radius.w;
  node.self_position_radius = Vec4f{self_p, self_r};
  node.child_position_radius = Vec4f{child_p, child_r};
}

void set_radii(VineRenderNode& node, float self_r, float child_r) {
  node.self_position_radius.w = self_r;
  node.child_position_radius.w = child_r;
}

VineAttachedToAggregateRenderData
to_aggregate_render_data(const VineAttachedToAggregateRenderDescriptor& desc) {
  VineAttachedToAggregateRenderData result{};
  result.wind_aabb_p0 = Vec4f{desc.wind_aabb_p0, 0.0f};
  result.wind_aabb_p1 = Vec4f{desc.wind_aabb_p1, 0.0f};
  return result;
}

VineRenderNode
to_render_node(const VineRenderNodeDescriptor& desc, const uint32_t* aggregate_indices) {
  VineRenderNode result{};
  result.self_position_radius = Vec4f{desc.self_p, desc.self_radius};
  result.child_position_radius = Vec4f{desc.child_p, desc.child_radius};
  encode_directions(
    desc.self_frame_right, desc.self_frame_up, desc.child_frame_right, desc.child_frame_up,
    &result.directions0, &result.directions1);
  result.self_aggregate_index_child_aggregate_index_unused = Vec4<uint32_t>{
    aggregate_indices[desc.self_aggregate_index],
    aggregate_indices[desc.child_aggregate_index],
    0, 0
  };
  result.wind_info0 = desc.wind_info0;
  result.wind_info1 = desc.wind_info1;
  result.wind_info2 = desc.wind_info2;
  return result;
}

uint32_t require_aggregate_slot(RenderVineSystem* sys, VineRenderSegmentHandle seg) {
  bool acquired{};
  uint32_t ind{};
  for (uint32_t i = 0; i < uint32_t(sys->aggregate_slot_state.size()); i++) {
    if (!sys->aggregate_slot_state[i].in_use) {
      acquired = true;
      ind = i;
      break;
    }
  }

  if (!acquired) {
    ind = uint32_t(sys->aggregate_slot_state.size());
    sys->aggregate_slot_state.emplace_back();
    sys->aggregate_render_data.emplace_back();
  }

  auto& state = sys->aggregate_slot_state[ind];
  assert(!state.in_use);
  state.in_use = true;
  state.segment_handle = seg;
  return ind;
}

} //  anon

VineRenderSegmentHandle
tree::create_vine_render_segment(RenderVineSystem* sys,
                                 const VineRenderNodeDescriptor* nodes, int num_nodes,
                                 const VineAttachedToAggregateRenderDescriptor* aggregates,
                                 int num_aggregates) {
  ContiguousElementGroupAllocator::ElementGroupHandle gh{};
  (void) sys->segment_alloc.reserve(num_nodes, &gh);

  VineRenderSegmentHandle result{};
  result.id = gh.index;

  Temporary<uint32_t, 1024> store_aggregate_indices;
  uint32_t* aggregate_indices = store_aggregate_indices.require(num_aggregates);
  for (uint32_t i = 0; i < uint32_t(num_aggregates); i++) {
    aggregate_indices[i] = require_aggregate_slot(sys, result);
    sys->aggregate_render_data[aggregate_indices[i]] = to_aggregate_render_data(aggregates[i]);
  }

  const auto off = uint32_t(sys->render_nodes.size());
  sys->render_nodes.resize(off + num_nodes);

  for (uint32_t i = 0; i < uint32_t(num_nodes); i++) {
    assert(nodes[i].self_aggregate_index < uint32_t(num_aggregates) &&
           nodes[i].child_aggregate_index < uint32_t(num_aggregates));
    sys->render_nodes[i + off] = to_render_node(nodes[i], aggregate_indices);
  }

  sys->render_nodes_modified = true;

  return result;
}

void tree::destroy_vine_render_segment(RenderVineSystem* sys, VineRenderSegmentHandle handle) {
  sys->segment_alloc.release(ContiguousElementGroupAllocator::ElementGroupHandle{handle.id});

  ContiguousElementGroupAllocator::Movement movement{};
  uint32_t tail{};
  (void) sys->segment_alloc.arrange_implicit(&movement, &tail);

  movement.apply(sys->render_nodes.data(), sizeof(VineRenderNode));
  sys->render_nodes.resize(tail);

  for (auto& slot : sys->aggregate_slot_state) {
    if (slot.segment_handle == handle) {
      slot.in_use = false;
      slot.segment_handle = {};
    }
  }

  sys->render_nodes_modified = true;
}

void tree::set_vine_node_positions(RenderVineSystem* sys, VineRenderSegmentHandle segment,
                                   int offset, const VineRenderNodeDescriptor* nodes,
                                   int num_nodes) {
  ContiguousElementGroupAllocator::ElementGroupHandle gh{segment.id};
  auto* group = sys->segment_alloc.read_group(gh);
  assert(group);
  assert(group->count >= uint32_t(num_nodes + offset));

  for (uint32_t i = 0; i < uint32_t(num_nodes); i++) {
    auto& curr_node = sys->render_nodes[i + group->offset + offset];
    auto& desc = nodes[i];
    set_positions(curr_node, desc.self_p, desc.child_p);
  }

  sys->render_nodes_modified = true;
}

void tree::set_vine_node_radii(RenderVineSystem* sys, VineRenderSegmentHandle segment,
                               int offset, const VineRenderNodeDescriptor* nodes, int num_nodes,
                               bool broadcast) {
  ContiguousElementGroupAllocator::ElementGroupHandle gh{segment.id};
  auto* group = sys->segment_alloc.read_group(gh);
  assert(group);
  assert(group->count >= uint32_t(num_nodes + offset));

  for (uint32_t i = 0; i < uint32_t(num_nodes); i++) {
    auto& curr_node = sys->render_nodes[i + group->offset + offset];
    auto& desc = broadcast ? nodes[0] : nodes[i];
    set_radii(curr_node, desc.self_radius, desc.child_radius);
  }

  sys->render_nodes_modified = true;
}

ArrayView<const VineRenderNode> tree::read_vine_render_nodes(const RenderVineSystem* sys) {
  return make_view(sys->render_nodes);
}

ArrayView<const VineAttachedToAggregateRenderData>
tree::read_vine_attached_to_aggregate_render_data(const RenderVineSystem* sys) {
  return make_view(sys->aggregate_render_data);
}

bool tree::test_clear_render_nodes_modified(RenderVineSystem* sys) {
  if (sys->render_nodes_modified) {
    sys->render_nodes_modified = false;
    return true;
  } else {
    return false;
  }
}

RenderVineSystem* tree::create_render_vine_system() {
  return new RenderVineSystem();
}

void tree::destroy_render_vine_system(RenderVineSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

GROVE_NAMESPACE_END
