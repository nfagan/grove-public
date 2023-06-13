#include "vine_system.hpp"
#include "render_vine_system.hpp"
#include "growth_on_nodes.hpp"
#include "tree_system.hpp"
#include "../bounds/bounds_system.hpp"
#include "render.hpp"
#include "utility.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/Temporary.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using UpdateInfo = VineSystemUpdateInfo;

struct Config {
  static constexpr int spiral_downsample_interval = 4;
};

struct VineSegmentGrowthContext {
  bool initialized;
  bool growing;
  int node_index;
  std::vector<int> pending_lateral_axes;
  float t;
};

struct VineSegmentTipData {
  static VineSegmentTipData missing() {
    VineSegmentTipData result{};
    result.wind_axis_root_info = WindAxisRootInfo::missing();
    result.src_aggregate_aabb.max = Vec3f{1.0f};
    return result;
  }

  WindAxisRootInfo wind_axis_root_info;
  Bounds3f src_aggregate_aabb;
};

struct VineSegment {
  VineSegmentHandle handle{};
  Optional<VineRenderSegmentHandle> render_segment;
  Optional<VineSegmentTipData> tip_data;
  int node_offset{};
  int node_size{};
  VineSegmentGrowthContext growth_context{};
  Optional<TreeInstanceHandle> associated_tree;
  Optional<VineSegmentHandle> grow_next_segment;
  bool finished_growing{};
};

struct StartNewVineOnTree {
  TreeInstanceHandle tree;
  VineSegmentHandle dst_segment;
  float spiral_theta;
};

struct JumpToNearbyTree {
  VineSystemTryToJumpToNearbyTreeParams params;
  VineSegmentHandle src_segment;
};

struct VineInstance {
  VineInstanceHandle handle{};
  float radius{0.05f};
  float growth_rate_scale{1.0f};
  std::vector<VineNode> nodes;
  std::vector<VineSegment> segments;
  DynamicArray<StartNewVineOnTree, 4> pending_new_vine_on_tree;
  DynamicArray<JumpToNearbyTree, 4> pending_jump_to_nearby_tree;
  bool need_start_destroying{};
  bool is_destroying{};
  Stopwatch stopwatch;
};

} //  anon

namespace tree {

struct VineSystem {
  std::vector<VineInstance> instances;
  uint32_t next_instance_id{1};
  uint32_t next_segment_id{1};
  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};
  float global_growth_rate_scale{1.0f};
  int min_num_segments_created_this_frame{};
  DynamicArray<VineInstanceHandle, 4> pending_destruction;
};

} //  tree

namespace {

Vec3<uint8_t> encode_normal(const Vec3f& n) {
  constexpr auto de = float(0xff);
  auto n01 = clamp_each(n * 0.5f + 0.5f, Vec3f{}, Vec3f{1.0f});
  auto r = n01 * de;
  return Vec3<uint8_t>{uint8_t(r.x), uint8_t(r.y), uint8_t(r.z)};
}

Vec3f decode_normal(const Vec3<uint8_t>& v) {
  auto vf = (to_vec3f(v) / 255.0f) * 2.0f - 1.0f;
  auto len = vf.length();
  return len > 0.0f ? vf / len : vf;
}

int axis_tip_index(const VineNode* nodes, int ni) {
  while (ni != -1) {
    auto& node = nodes[ni];
    if (node.has_medial_child()) {
      ni = node.medial_child;
    } else {
      return ni;
    }
  }
  return -1;
}

VineSegment* find_segment(std::vector<VineSegment>& segments, VineSegmentHandle handle) {
  for (auto& seg : segments) {
    if (seg.handle == handle) {
      return &seg;
    }
  }
  return nullptr;
}

const VineSegment* find_segment(const std::vector<VineSegment>& segments, VineSegmentHandle handle) {
  for (auto& seg : segments) {
    if (seg.handle == handle) {
      return &seg;
    }
  }
  return nullptr;
}

VineInstance* find_instance(VineSystem* sys, VineInstanceHandle handle) {
  for (auto& inst : sys->instances) {
    if (inst.handle == handle) {
      return &inst;
    }
  }
  return nullptr;
}

const VineInstance* find_instance(const VineSystem* sys, VineInstanceHandle handle) {
  for (auto& inst : sys->instances) {
    if (inst.handle == handle) {
      return &inst;
    }
  }
  return nullptr;
}

VineSegmentHandle reserve_segment(VineSystem* sys, VineInstance& inst, VineSegment** dst_segment) {
  VineSegmentHandle segment_handle{sys->next_segment_id++};

  VineSegment segment{};
  segment.handle = segment_handle;
  segment.node_offset = -1;

  inst.segments.emplace_back() = segment;
  *dst_segment = &inst.segments.back();
  return segment_handle;
}

void decompose_internodes(const Internode* nodes, int num_nodes, OBB3f* bounds,
                          int* medial_children, int* parents) {
  tree::internode_obbs(nodes, num_nodes, bounds);
  for (int i = 0; i < num_nodes; i++) {
    medial_children[i] = nodes[i].medial_child;
    parents[i] = nodes[i].parent;
  }
}

tree::Internode make_line_as_node(const Vec3f& p0, const Vec3f& p1, float radius) {
  tree::Internode result{};
  result.direction = normalize(p1 - p0);
  result.position = p0;
  result.length = (p1 - p0).length();
  result.diameter = radius * 2.0f;
  return result;
}

SpiralAroundNodesParams make_spiral_around_nodes_params(int init_ni, float theta) {
  tree::SpiralAroundNodesParams spiral_params{};
  spiral_params.init_ni = init_ni;
  spiral_params.step_size = 0.1f;
  spiral_params.step_size_randomness = 0.0f;
//  spiral_params.theta = pif() * 0.25f;
  spiral_params.theta = theta;
  spiral_params.theta_randomness = 0.0f;
  spiral_params.n_off = 0.1f;
  spiral_params.randomize_initial_position = false;
  spiral_params.disable_node_intersect_check = false;
  return spiral_params;
}

int compute_spiral_around_nodes(const Internode* nodes, int num_nodes,
                                const SpiralAroundNodesParams& spiral_params,
                                int downsample_interval, SpiralAroundNodesEntry* dst_entries,
                                int max_num_entries) {
  Temporary<int, 2048> store_med_children;
  Temporary<int, 2048> store_parents;
  Temporary<OBB3f, 2048> store_bounds;

  int* med_children = store_med_children.require(num_nodes);
  int* parents = store_parents.require(num_nodes);
  OBB3f* node_bounds = store_bounds.require(num_nodes);
  decompose_internodes(nodes, num_nodes, node_bounds, med_children, parents);

  int num_entries = spiral_around_nodes(
    node_bounds, med_children, parents, num_nodes, spiral_params, max_num_entries, dst_entries);

  num_entries = downsample_spiral_around_nodes_entries(
    dst_entries, num_entries, node_bounds, num_nodes, downsample_interval);
#if 1
  num_entries = keep_spiral_until_first_node_intersection(
    dst_entries, num_entries, node_bounds, num_nodes);
#endif
  return num_entries;
}

void to_vine_nodes(const Internode* src_nodes, const Vec3f* ns, int num_src,
                   int index_offset, VineNode* dst_nodes) {
  for (int i = 0; i < num_src; i++) {
    auto& src = src_nodes[i];
    auto& dst = dst_nodes[i];
    assert(src.medial_child < num_src && src.lateral_child < num_src && src.parent < num_src);
    dst = {};
    dst.position = src.position;
    dst.direction = src.direction;
    dst.radius = src.radius();
    dst.parent = src.parent == -1 ? -1 : src.parent + index_offset;
    dst.medial_child = src.medial_child == -1 ? -1 : src.medial_child + index_offset;
    dst.lateral_child = src.lateral_child == -1 ? -1 : src.lateral_child + index_offset;
    dst.attached_surface_normal = encode_normal(ns[i]);
    dst.attached_node_index = i;
  }
}

void to_vine_nodes(const SpiralAroundNodesEntry* entries, const Internode* nodes,
                   int num_entries, int index_offset, float radius, VineNode* dst) {
  for (int i = 0; i < num_entries; i++) {
    auto& src = entries[i];
    auto& node = dst[i];
    node = {};
    node.radius = radius;
    node.parent = i == 0 ? -1 : index_offset + i - 1;
    node.medial_child = i + 1 < num_entries ? index_offset + i + 1 : -1;
    node.lateral_child = -1;
    node.position = src.p;
    node.direction = i + 1 < num_entries ?
      normalize(entries[i + 1].p - src.p) :
      nodes ? nodes[src.node_index].direction : Vec3f{1.0f, 0.0f, 0.0f};
    node.attached_surface_normal = encode_normal(src.n);
    node.attached_node_index = src.node_index;
  }
}

void to_render_nodes_from_internodes_no_wind(const Internode* nodes, const Mat3f* node_frames,
                                             int num_nodes, VineRenderNodeDescriptor* descs,
                                             bool hidden = true) {
  for (int i = 0; i < num_nodes; i++) {
    auto& src = nodes[i];

    const Mat3f* self_frame = node_frames + i;
    Vec3f self_p = src.position;
    float self_r = src.radius();

    const Mat3f* child_frame = self_frame;
    Vec3f child_p = self_p;
    float child_r = self_r;

    if (src.has_medial_child()) {
      child_frame = node_frames + src.medial_child;
      child_p = nodes[src.medial_child].position;
      child_r = nodes[src.medial_child].radius();
    }

    VineRenderNodeDescriptor desc{};
    if (!hidden) {
      desc.self_p = self_p;
      desc.child_p = child_p;
    }
    desc.self_radius = self_r;
    desc.child_radius = child_r;
    desc.self_frame_right = (*self_frame)[0];
    desc.self_frame_up = (*self_frame)[1];
    desc.child_frame_right = (*child_frame)[0];
    desc.child_frame_up = (*child_frame)[1];
    descs[i] = desc;
  }
}

void to_render_nodes(const VineNode* nodes, const WindAxisRootInfo* wind_info,
                     const uint32_t* aggregate_indices, int num_nodes,
                     VineRenderNodeDescriptor* descs) {
  assert(wind_info);
  for (int i = 0; i < num_nodes; i++) {
    auto& src = nodes[i];

    Vec3f self_i;
    Vec3f self_j;
    Vec3f self_k;
    Vec3f self_p = src.position;
    make_coordinate_system_y(src.direction, &self_i, &self_j, &self_k);
    float self_radius = src.radius;

    Vec3f child_i = self_i;
    Vec3f child_j = self_j;
    Vec3f child_k = self_k;
    Vec3f child_p = self_p;
    float child_radius = self_radius;
    if (i + 1 < num_nodes) {
      auto& next = nodes[i + 1];
      make_coordinate_system_y(next.direction, &child_i, &child_j, &child_k);
      child_p = next.position;
      child_radius = next.radius;
    }

    VineRenderNodeDescriptor desc{};
    desc.self_radius = self_radius;
    desc.child_radius = child_radius;
//    desc.self_p = self_p;
//    desc.child_p = child_p;
    desc.self_frame_right = self_i;
    desc.self_frame_up = self_j;
    desc.child_frame_right = child_i;
    desc.child_frame_up = child_j;

    auto packed_info = to_packed_wind_info(
      wind_info[i], i + 1 < num_nodes ? wind_info[i + 1] : wind_info[i]);
    desc.wind_info0 = packed_info[0];
    desc.wind_info1 = packed_info[1];
    desc.wind_info2 = packed_info[2];

    if (aggregate_indices) {
      const uint32_t self_agg_ind = aggregate_indices[i];
      desc.self_aggregate_index = self_agg_ind;
      desc.child_aggregate_index = i + 1 < num_nodes ? aggregate_indices[i + 1] : self_agg_ind;
    }

    descs[i] = desc;
  }
}

Optional<Vec3f>
make_segment_along_internodes(VineInstance& inst, VineSegment& segment,
                              const tree::Internodes& internodes, const Bounds3f& src_aabb,
                              const SpiralAroundNodesParams& spiral_params, const UpdateInfo& info) {
  const auto* src_nodes = internodes.data();
  const auto num_src_nodes = int(internodes.size());

  constexpr int max_num_entries = 1024;
  SpiralAroundNodesEntry entries[max_num_entries];

  const int num_entries = compute_spiral_around_nodes(
    src_nodes, num_src_nodes, spiral_params, Config::spiral_downsample_interval,
    entries, max_num_entries);

  const int offset = int(inst.nodes.size());
  inst.nodes.resize(offset + num_entries);
  to_vine_nodes(entries, src_nodes, num_entries, offset, inst.radius, inst.nodes.data() + offset);

  assert(segment.node_offset == -1 && segment.node_size == 0);
  segment.node_offset = offset;
  segment.node_size = num_entries;

  Vec3f first_p;
  if (num_entries == 0) {
    return NullOpt{};
  } else {
    first_p = entries[0].p;
  }

  WindAxisRootInfo wind_root_infos[max_num_entries];
  {
    auto axis_root_info = tree::compute_axis_root_info(internodes);
    auto remapped_roots = tree::remap_axis_roots(internodes);
    for (int i = 0; i < num_entries; i++) {
      const int ni = entries[i].node_index;
      wind_root_infos[i] = make_wind_axis_root_info(
        internodes[ni], internodes, axis_root_info, remapped_roots, src_aabb);
    }
  }

  VineRenderNodeDescriptor render_descs[max_num_entries];
  auto* src_vine_nodes = inst.nodes.data() + offset;
  to_render_nodes(src_vine_nodes, wind_root_infos, nullptr, num_entries, render_descs);

  VineAttachedToAggregateRenderDescriptor aggregate_desc{};
  aggregate_desc.wind_aabb_p0 = src_aabb.min;
  aggregate_desc.wind_aabb_p1 = src_aabb.max;

  assert(!segment.render_segment);
  segment.render_segment = tree::create_vine_render_segment(
    info.render_vine_system, render_descs, num_entries, &aggregate_desc, 1);

  VineSegmentTipData tip_data{};
  tip_data.wind_axis_root_info = wind_root_infos[num_entries - 1];
  tip_data.src_aggregate_aabb = src_aabb;
  segment.tip_data = std::move(tip_data);

  return Optional<Vec3f>(first_p);
}

void make_segment_between_internodes(VineInstance& inst, VineSegment& segment,
                                     const WindAxisRootInfo& src_node_root_info,
                                     const Bounds3f& src_aabb, const Vec3f& src_p,
                                     const Optional<Vec3f>& connect_to_src_p,
                                     const WindAxisRootInfo& dst_node_root_info,
                                     const Bounds3f& dst_aabb, const Vec3f& dst_p,
                                     const SpiralAroundNodesParams& spiral_params,
                                     const VineSystemUpdateInfo& info) {
  constexpr int max_num_entries = 1024;
  SpiralAroundNodesEntry store_entries[max_num_entries];
  //  Reserve first slot for `connect_to_src_p`
  SpiralAroundNodesEntry* entries = store_entries + 1;

  auto connect_inode = make_line_as_node(src_p, dst_p, 0.25f);
  int num_entries = compute_spiral_around_nodes(
    &connect_inode, 1, spiral_params, Config::spiral_downsample_interval,
    //  -1 for first, -1 for last, -1 for possibility of connect_to_src_p
    entries + 1, max_num_entries - 3);

  entries[0] = SpiralAroundNodesEntry{src_p, {}, -1};
  entries[num_entries + 1] = SpiralAroundNodesEntry{dst_p, {}, -1};
  num_entries += 2;

  if (connect_to_src_p) {
    //  Put `connect_to_src_p` first.
    entries = store_entries;
    entries[0] = SpiralAroundNodesEntry{connect_to_src_p.value(), {}, -1};
    num_entries++;
  }

  const int offset = int(inst.nodes.size());
  inst.nodes.resize(offset + num_entries);
  to_vine_nodes(entries, nullptr, num_entries, offset, inst.radius, inst.nodes.data() + offset);

  const int n_back = std::min(num_entries, 4);
  WindAxisRootInfo wind_root_infos[max_num_entries];
  uint32_t aggregate_indices[max_num_entries];
  for (int i = 0; i < num_entries - n_back; i++) {
    wind_root_infos[i] = src_node_root_info;
    aggregate_indices[i] = 0;
  }

  for (int i = 0; i < n_back; i++) {
    wind_root_infos[num_entries - n_back + i] = dst_node_root_info;
    aggregate_indices[num_entries - n_back + i] = 1;
  }

  VineRenderNodeDescriptor render_descs[max_num_entries];
  auto* src_vine_nodes = inst.nodes.data() + offset;
  to_render_nodes(src_vine_nodes, wind_root_infos, aggregate_indices, num_entries, render_descs);

  VineAttachedToAggregateRenderDescriptor aggregate_descs[2]{};
  aggregate_descs[0].wind_aabb_p0 = src_aabb.min;
  aggregate_descs[0].wind_aabb_p1 = src_aabb.max;
  aggregate_descs[1].wind_aabb_p0 = dst_aabb.min;
  aggregate_descs[1].wind_aabb_p1 = dst_aabb.max;

  assert(!segment.render_segment);
  assert(segment.node_offset == -1 && segment.node_size == 0);
  segment.render_segment = tree::create_vine_render_segment(
    info.render_vine_system, render_descs, num_entries, aggregate_descs, 2);
  segment.node_offset = offset;
  segment.node_size = num_entries;
}

#if 1

struct JumpToNearbyTreeCandidate {
  tree::TreeInstanceHandle instance;
  int hit_leaf_index;
  Vec3f leaf_p;
  float distance_to_leaf;
};

OBB3f make_jump_candidate_bounds(const JumpToNearbyTreeCandidate& candidate, const Vec3f& p0) {
  const float dist2 = candidate.distance_to_leaf * 0.5f;
  const float xz_dist = 0.0125f;
  auto axis = candidate.leaf_p - p0;
  axis /= candidate.distance_to_leaf;
  OBB3f eval_bounds;
  make_coordinate_system_y(normalize(axis), &eval_bounds.i, &eval_bounds.j, &eval_bounds.k);
  eval_bounds.position = p0 + axis * dist2;
  eval_bounds.half_size = Vec3f{xz_dist, dist2, xz_dist};
  return eval_bounds;
}

auto find_tree_to_jump_to(const TreeSystem* tree_system, const bounds::Accel* accel,
                          const OBB3f& examine_bounds, const Vec3f& init_p,
                          const TreeInstanceHandle& source_instance,
                          bounds::ElementTag arch_element_tag) {
  struct Result {
    Optional<tree::TreeInstanceHandle> closest_leaf_tree_instance;
    Optional<int> closest_leaf_index;
  };

  std::vector<const bounds::Element*> bounds_elements;
  accel->intersects(bounds::make_query_element(examine_bounds), bounds_elements);

  constexpr int max_num_candidates = 8;
  JumpToNearbyTreeCandidate candidates[max_num_candidates]{};
  int num_candidates{};

  tree::Internode hit_internode;
  for (auto& el : bounds_elements) {
    tree::TreeInstanceHandle hit_inst;
    int hit_internode_index;
    bool found_inst = tree::lookup_by_bounds_element_ids(
      tree_system,
      bounds::ElementID{el->parent_id},
      bounds::ElementID{el->id},
      &hit_inst, &hit_internode, &hit_internode_index);

    if (!found_inst || hit_inst == source_instance || !hit_internode.is_leaf()) {
      continue;
    }

    const Vec3f leaf_p = hit_internode.position;
    const float dist = (leaf_p - init_p).length();
    int insert_at{};
    for (; insert_at < num_candidates; ++insert_at) {
      if (dist < candidates[insert_at].distance_to_leaf) {
        break;
      }
    }

    num_candidates = std::min(num_candidates + 1, max_num_candidates);
    for (int i = num_candidates - 1; i > insert_at; i--) {
      candidates[i] = candidates[i - 1];
    }
    if (insert_at < num_candidates) {
      candidates[insert_at] = {hit_inst, hit_internode_index, leaf_p, dist};
    }
  }

  Result result{};
  for (int i = 0; i < num_candidates; i++) {
    auto& candidate = candidates[i];
    if (candidate.distance_to_leaf <= 0.0f) {
      continue;
    }

    const auto eval_bounds = make_jump_candidate_bounds(candidate, init_p);
    bounds_elements.clear();
    accel->intersects(bounds::make_query_element(eval_bounds), bounds_elements);

    bool reject_candidate{};
    for (auto& hit : bounds_elements) {
      if (hit->tag == arch_element_tag.id) {
        reject_candidate = true;
        break;
      }
    }

    if (!reject_candidate) {
      result.closest_leaf_tree_instance = candidate.instance;
      result.closest_leaf_index = candidate.hit_leaf_index;
      break;
    }
  }

  return result;
}
#else
auto find_tree_to_jump_to(const TreeSystem* tree_system, const bounds::Accel* accel,
                          const OBB3f& examine_bounds, const Vec3f& init_p,
                          const TreeInstanceHandle& source_instance,
                          bounds::ElementTag) {
  struct Result {
    Optional<tree::TreeInstanceHandle> closest_leaf_tree_instance;
    Optional<int> closest_leaf_index;
  };

  std::vector<const bounds::Element*> bounds_elements;
  accel->intersects(bounds::make_query_element(examine_bounds), bounds_elements);

  Optional<tree::TreeInstanceHandle> closest_leaf_tree_instance;
  Optional<int> closest_leaf_index;
  tree::Internode hit_internode;
  float closest_leaf_distance{infinityf()};

  for (auto& el : bounds_elements) {
    tree::TreeInstanceHandle hit_inst;
    int hit_internode_index;
    bool found_inst = tree::lookup_by_bounds_element_ids(
      tree_system,
      bounds::ElementID{el->parent_id},
      bounds::ElementID{el->id},
      &hit_inst, &hit_internode, &hit_internode_index);

    if (found_inst && hit_inst != source_instance && hit_internode.is_leaf()) {
      float dist = (hit_internode.position - init_p).length();
      if (dist < closest_leaf_distance) {
        closest_leaf_tree_instance = hit_inst;
        closest_leaf_index = hit_internode_index;
        closest_leaf_distance = dist;
      }
    }
  }

  Result result;
  result.closest_leaf_tree_instance = closest_leaf_tree_instance;
  result.closest_leaf_index = closest_leaf_index;
  return result;
}
#endif

void update_new_vine(VineSystem* sys, VineInstance& inst, const VineSystemUpdateInfo& info) {
  if (inst.pending_new_vine_on_tree.empty() || sys->min_num_segments_created_this_frame > 0) {
    return;
  }

  const auto pend = inst.pending_new_vine_on_tree[0];

  auto tree = read_tree(info.tree_system, pend.tree);
  if (!tree.nodes || tree.growth_state.modifying != TreeSystem::ModifyingState::Idle) {
    return;
  }

  auto* segment = find_segment(inst.segments, pend.dst_segment);
  assert(segment);

  auto spiral_params = make_spiral_around_nodes_params(0, pend.spiral_theta);
  make_segment_along_internodes(
    inst, *segment, tree.nodes->internodes, *tree.src_aabb, spiral_params, info);
  segment->growth_context.growing = true;

  inst.pending_new_vine_on_tree.erase(inst.pending_new_vine_on_tree.begin());

  sys->min_num_segments_created_this_frame++;
}

void update_jump_to_nearby_tree(VineSystem* sys, VineInstance& inst, const UpdateInfo& info) {
  if (inst.pending_jump_to_nearby_tree.empty() || sys->min_num_segments_created_this_frame > 0) {
    return;
  }

  const auto pend = inst.pending_jump_to_nearby_tree[0];
  auto* src_seg = find_segment(inst.segments, pend.src_segment);
  assert(src_seg);

  if (src_seg->growth_context.growing) {
    //  Wait until segment has finished growing
    return;
  }

  if (src_seg->node_offset < 0) {
    //  Nodes not created yet.
    return;
  } else if (src_seg->node_size == 0) {
    //  No nodes to jump from. This is technically probably ok, but want to know if this happens.
    assert(false);
    inst.pending_jump_to_nearby_tree.erase(inst.pending_jump_to_nearby_tree.begin());
    return;
  }

  auto* accel = request_read(info.bounds_system, info.accel_handle, sys->bounds_accessor_id);
  if (!accel) {
    return;
  }

  Vec3f init_p;
  Optional<Vec3f> connect_to_init_p;
  {
    const int last_node_index = axis_tip_index(inst.nodes.data(), src_seg->node_offset);
    assert(last_node_index < int(inst.nodes.size()));
    const auto& last_node = inst.nodes[last_node_index];
    init_p = last_node.position;

    if (pend.params.use_initial_offset) {
      connect_to_init_p = init_p;
      init_p += pend.params.initial_offset;
    }
  }

  const TreeInstanceHandle source_instance = src_seg->associated_tree ?
    src_seg->associated_tree.value() : TreeInstanceHandle{};

  const float examine_radius = 8.0f;
  auto examine_bounds = OBB3f::axis_aligned(init_p, Vec3f{examine_radius});
  auto jump_res = find_tree_to_jump_to(
    info.tree_system, accel, examine_bounds, init_p, source_instance, info.arch_bounds_element_tag);

  if (jump_res.closest_leaf_tree_instance) {
    const auto closest_inst_handle = jump_res.closest_leaf_tree_instance.value();
    const auto closest_inst = tree::read_tree(info.tree_system, closest_inst_handle);
    //  Shouldn't be able to find this instance (above) if its nodes are unavailable to read.
    assert(closest_inst.nodes && jump_res.closest_leaf_index);
    const int closest_leaf_ind = jump_res.closest_leaf_index.value();
    assert(closest_leaf_ind < int(closest_inst.nodes->internodes.size()));
    auto& closest_leaf = closest_inst.nodes->internodes[closest_leaf_ind];
    assert(closest_leaf.is_leaf());

    Vec3f first_next_p = closest_leaf.position;
    VineSegmentHandle next_seg_handle{};
    { //  segment down next tree
      VineSegment* dst_seg{};
      next_seg_handle = reserve_segment(sys, inst, &dst_seg);
      dst_seg->associated_tree = closest_inst_handle;

      const float down_theta = pif() * 0.25f + pif();
      auto spiral_params = make_spiral_around_nodes_params(closest_leaf_ind, down_theta);

      auto& close_nodes = closest_inst.nodes->internodes;
      auto& close_aabb = *closest_inst.src_aabb;
      auto maybe_first_next_p = make_segment_along_internodes(
        inst, *dst_seg, close_nodes, close_aabb, spiral_params, info);
      if (maybe_first_next_p) {
        first_next_p = maybe_first_next_p.value();
      }
    }

    { //  connecting segment
      VineSegment* dst_seg{};
      auto dst_seg_handle = reserve_segment(sys, inst, &dst_seg);
      (void) dst_seg_handle;

      auto& dst_nodes = closest_inst.nodes->internodes;
      auto& dst_aabb = *closest_inst.src_aabb;
      auto dst_axis_root_info = tree::compute_axis_root_info(dst_nodes);
      auto dst_remapped_roots = tree::remap_axis_roots(dst_nodes);
      auto dst_root_info = make_wind_axis_root_info(
        closest_leaf, dst_nodes, dst_axis_root_info, dst_remapped_roots, dst_aabb);

      const auto spiral_params = make_spiral_around_nodes_params(0, pif() * 0.25f);

      //  reacquire ptr to source segment, because we pushed new segments
      src_seg = find_segment(inst.segments, pend.src_segment);
      const auto src_tip = src_seg->tip_data ?
        src_seg->tip_data.value() : VineSegmentTipData::missing();
      make_segment_between_internodes(
        inst, *dst_seg, src_tip.wind_axis_root_info,
        src_tip.src_aggregate_aabb, init_p, connect_to_init_p,
        dst_root_info, dst_aabb, first_next_p, spiral_params, info);

      dst_seg->growth_context.growing = true;
      dst_seg->grow_next_segment = next_seg_handle;
    }
  }

  bounds::release_read(info.bounds_system, info.accel_handle, sys->bounds_accessor_id);
  inst.pending_jump_to_nearby_tree.erase(inst.pending_jump_to_nearby_tree.begin());

  sys->min_num_segments_created_this_frame++;
}

void grow_segment(VineSystem* sys, VineInstance& inst, VineSegment& seg, const UpdateInfo& info) {
  auto& ctx = seg.growth_context;
  if (!ctx.growing || seg.node_size == 0) {
    return;
  }

  if (!ctx.initialized) {
    ctx.node_index = seg.node_offset;
    ctx.initialized = true;
  }

  const int ni = ctx.node_index;
  assert(ni >= 0 && ni < seg.node_offset + seg.node_size && ni < int(inst.nodes.size()));

  auto& node = inst.nodes[ni];
  Vec3f p0 = node.position;
  Vec3f p1 = p0;
  if (node.has_medial_child()) {
    auto& med = inst.nodes[node.medial_child];
    p1 = med.position;
  }

  const float dist = std::max(1e-3f, (p1 - p0).length());
  const float dist_scale = 1.0f / dist;
  const float global_scale = sys->global_growth_rate_scale;
  const float growth_rate_scale = inst.growth_rate_scale * global_scale * dist_scale;

  ctx.t = clamp(float(ctx.t + info.real_dt * growth_rate_scale), 0.0f, 1.0f);
  const Vec3f child_p = lerp(ctx.t, p0, p1);

  const bool finished_node_growth = ctx.t == 1.0f;
  if (finished_node_growth) {
    ctx.t = 0.0f;

    if (node.has_lateral_child()) {
      ctx.pending_lateral_axes.push_back(node.lateral_child);
    }

    if (node.has_medial_child()) {
      ctx.node_index = node.medial_child;

    } else if (!ctx.pending_lateral_axes.empty()) {
      ctx.node_index = ctx.pending_lateral_axes.back();
      ctx.pending_lateral_axes.pop_back();

    } else {
      ctx = {};
      //  finished growing
      seg.finished_growing = true;
      if (seg.grow_next_segment) {
        if (auto* next_seg = find_segment(inst.segments, seg.grow_next_segment.value())) {
          next_seg->growth_context.growing = true;
        }
        seg.grow_next_segment = NullOpt{};
      }
    }
  }

  if (seg.render_segment) {
    auto& render_seg = seg.render_segment.value();

    VineRenderNodeDescriptor render_desc{};
    render_desc.self_p = node.position;
    render_desc.child_p = child_p;

    assert(ni >= seg.node_offset);
    const int ri = ni - seg.node_offset;
    set_vine_node_positions(info.render_vine_system, render_seg, ri, &render_desc, 1);
  }
}

void update_growing(VineSystem* sys, VineInstance& inst, const UpdateInfo& info) {
  for (auto& seg : inst.segments) {
    grow_segment(sys, inst, seg, info);
  }
}

void update_destroying(VineSystem* sys, VineInstance& inst, bool init_destroy,
                       const VineSystemUpdateInfo& info) {
  if (init_destroy) {
    inst.stopwatch.reset();
  }

  const double t_destroy = 0.5;
  double t = std::min(inst.stopwatch.delta().count(), t_destroy) / t_destroy;
  const auto r = float(inst.radius * (1.0 - t));

  for (auto& seg : inst.segments) {
    if (seg.render_segment) {
      VineRenderNodeDescriptor desc{};
      desc.self_radius = r;
      desc.child_radius = r;
      set_vine_node_radii(
        info.render_vine_system, seg.render_segment.value(), 0, &desc, seg.node_size, true);
    }
  }

  if (t == 1.0) {
    auto& pend = sys->pending_destruction;
    auto it = std::find(pend.begin(), pend.end(), inst.handle);
    if (it == pend.end()) {
      pend.push_back(inst.handle);
    }
  }
}

void update_instance(VineSystem* sys, VineInstance& inst, const VineSystemUpdateInfo& info) {
  bool init_destroy{};
  if (inst.need_start_destroying) {
    inst.is_destroying = true;
    inst.need_start_destroying = false;
    init_destroy = true;
  }

  if (inst.is_destroying) {
    update_destroying(sys, inst, init_destroy, info);

  } else {
    update_new_vine(sys, inst, info);
    update_jump_to_nearby_tree(sys, inst, info);
    update_growing(sys, inst, info);
  }
}

void destroy_instance(VineSystem* sys, VineInstanceHandle handle, const VineSystemUpdateInfo& info) {
  auto* inst = find_instance(sys, handle);
  assert(inst);

  for (auto& seg : inst->segments) {
    if (seg.render_segment) {
      tree::destroy_vine_render_segment(info.render_vine_system, seg.render_segment.value());
    }
  }

  const auto inst_ind = int(inst - sys->instances.data());
  sys->instances.erase(sys->instances.begin() + inst_ind);
}

void destroy_pending(VineSystem* sys, const VineSystemUpdateInfo& info) {
  for (VineInstanceHandle handle : sys->pending_destruction) {
    destroy_instance(sys, handle, info);
  }
  sys->pending_destruction.clear();
}

} //  anon

VineInstanceHandle tree::create_vine_instance(VineSystem* sys, float radius) {
  auto& inst = sys->instances.emplace_back();
  inst.radius = radius;
  inst.handle = VineInstanceHandle{sys->next_instance_id++};
  return inst.handle;
}

void tree::destroy_vine_instance(VineSystem* sys, VineInstanceHandle handle) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  if (!inst->is_destroying) {
    inst->need_start_destroying = true;
  }
}

bool tree::vine_exists(const VineSystem* sys, VineInstanceHandle inst) {
  return find_instance(sys, inst) != nullptr;
}

VineSegmentHandle tree::start_new_vine_on_tree(VineSystem* sys, VineInstanceHandle handle,
                                               TreeInstanceHandle tree, float spiral_theta) {
  auto* inst = find_instance(sys, handle);
  assert(inst);

  VineSegment* seg{};
  auto segment_handle = reserve_segment(sys, *inst, &seg);
  seg->associated_tree = tree;

  StartNewVineOnTree start_vine{};
  start_vine.tree = tree;
  start_vine.dst_segment = segment_handle;
  start_vine.spiral_theta = spiral_theta;
  inst->pending_new_vine_on_tree.push_back(start_vine);

  return segment_handle;
}

VineSegmentHandle tree::emplace_vine_from_internodes(
  VineSystem* sys, RenderVineSystem* render_sys, VineInstanceHandle handle,
  const Internode* internodes, const Vec3f* surface_ns, int num_internodes) {
  //
  auto* inst = find_instance(sys, handle);
  assert(inst);

  VineSegment* seg{};
  auto segment_handle = reserve_segment(sys, *inst, &seg);

  const auto node_off = int(inst->nodes.size());
  inst->nodes.resize(node_off + num_internodes);
  to_vine_nodes(internodes, surface_ns, num_internodes, node_off, inst->nodes.data() + node_off);

  seg->node_offset = node_off;
  seg->node_size = num_internodes;

  Temporary<Mat3f, 2048> store_node_frames;
  auto* node_frames = store_node_frames.require(num_internodes);
  compute_internode_frames(internodes, num_internodes, node_frames);

  Temporary<VineRenderNodeDescriptor, 2048> store_descs;
  auto* render_descs = store_descs.require(num_internodes);
  to_render_nodes_from_internodes_no_wind(internodes, node_frames, num_internodes, render_descs);

  const auto node_aabb = tree::internode_aabb(internodes, uint32_t(num_internodes));
  VineAttachedToAggregateRenderDescriptor aggregate_desc{};
  aggregate_desc.wind_aabb_p0 = node_aabb.min;
  aggregate_desc.wind_aabb_p1 = node_aabb.max;

  assert(!seg->render_segment);
  seg->render_segment = create_vine_render_segment(
    render_sys, render_descs, num_internodes, &aggregate_desc, 1);

  seg->growth_context.growing = true;

  return segment_handle;
}

void tree::try_to_jump_to_nearby_tree(VineSystem* sys, VineInstanceHandle handle,
                                      VineSegmentHandle segment,
                                      const VineSystemTryToJumpToNearbyTreeParams& params) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  JumpToNearbyTree jump{};
  jump.params = params;
  jump.src_segment = segment;
  inst->pending_jump_to_nearby_tree.push_back(jump);
}

void tree::set_growth_rate_scale(VineSystem* sys, VineInstanceHandle handle, float s) {
  auto* inst = find_instance(sys, handle);
  assert(inst);
  inst->growth_rate_scale = std::max(0.0f, s);
}

void tree::set_global_growth_rate_scale(VineSystem* sys, float v) {
  assert(v >= 0.0f);
  sys->global_growth_rate_scale = v;
}

VineSystemStats tree::get_stats(const VineSystem* sys) {
  VineSystemStats result{};
  result.num_instances = int(sys->instances.size());
  for (auto& inst : sys->instances) {
    result.num_segments += int(inst.segments.size());
    result.num_nodes += int(inst.nodes.size());
  }
  return result;
}

float tree::get_global_growth_rate_scale(const VineSystem* sys) {
  return sys->global_growth_rate_scale;
}

void tree::update_vine_system(VineSystem* sys, const VineSystemUpdateInfo& info) {
  sys->min_num_segments_created_this_frame = 0;

  destroy_pending(sys, info);

  for (auto& inst : sys->instances) {
    update_instance(sys, inst, info);
  }
}

VineSystem* tree::create_vine_system() {
  return new VineSystem();
}

void tree::destroy_vine_system(VineSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

ReadVineSegment
tree::read_vine_segment(const VineSystem* sys, VineInstanceHandle inst, VineSegmentHandle seg) {
  //
  ReadVineSegment result{};

  auto* instance = find_instance(sys, inst);
  assert(instance);
  if (!instance->pending_new_vine_on_tree.empty()) {
    return result;
  }

  auto* segment = find_segment(instance->segments, seg);
  assert(segment);
  int beg = segment->node_offset;
  int end = beg + segment->node_size;
  assert(beg >= 0 && end <= int(instance->nodes.size()));

  if (segment->associated_tree) {
    assert(segment->associated_tree.value().id != 0);
    result.maybe_associated_tree_instance_id = segment->associated_tree.value().id;
  }

  result.node_beg = beg;
  result.node_end = end;
  result.nodes = instance->nodes.data();
  result.finished_growing = segment->finished_growing;
  return result;
}

Vec3f tree::VineNode::decode_attached_surface_normal() const {
  return decode_normal(attached_surface_normal);
}

GROVE_NAMESPACE_END
