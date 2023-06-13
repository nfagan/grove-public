#include "render_roots_system.hpp"
#include "roots_render.hpp"
#include "fit_growing_root_bounds.hpp"
#include "../render/roots_drawable_components.hpp"
#include "../render/render_branch_nodes_types.hpp"
#include "../render/frustum_cull_data.hpp"
#include "grove/common/common.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/ArrayView.hpp"
#include <unordered_map>

/*
 * @TODO (04/02/23) - There is a bug where the node AABBs appear to either become corrupted or
 * otherwise incorrect after pruning.
 */

GROVE_NAMESPACE_BEGIN

namespace tree {

using UpdateInfo = RenderRootsSystemUpdateInfo;

struct Config {
  static constexpr int nodes_per_growing_drawable = 64;
  static constexpr int cull_group_pool_size = 64;
};

struct RenderRootsInstance {
  RootsInstanceHandle associated_roots{};
  DynamicArray<TreeRootsDrawableComponents, 8> growing_drawable_component_pool;
  DynamicArray<cull::FrustumCullGroupHandle, 8> cull_groups;
  ExpandingBoundsSets expanding_bounds_sets;
  int num_growing_nodes_filled_cull_data{};
  bool need_destroy{};
  bool need_update_growth{};
  bool need_update_recede{};
  bool need_refill_receded{};
};

struct RenderRootsSystem {
  std::unordered_map<uint32_t, RenderRootsInstance> instances;
  uint32_t next_instance_id{1};
};

} //  tree

namespace {

using namespace tree;

RenderRootsInstance make_instance(const CreateRenderRootsInstanceParams& params) {
  RenderRootsInstance result{};
  result.associated_roots = params.associated_roots;
  return result;
}

void destroy_instance(RenderRootsInstance& inst, RenderBranchNodesData* rd,
                      cull::FrustumCullData* cull_data) {
  for (auto& components : inst.growing_drawable_component_pool) {
    tree::destroy_tree_roots_drawable_components(rd, &components);
  }
  inst.growing_drawable_component_pool.clear();

  for (auto& handle : inst.cull_groups) {
    cull::destroy_frustum_cull_instance_group(cull_data, handle);
  }
  inst.cull_groups.clear();
}

void destroy_pending(RenderRootsSystem* sys, const UpdateInfo& info) {
  auto it = sys->instances.begin();
  while (it != sys->instances.end()) {
    if (it->second.need_destroy) {
      destroy_instance(it->second, info.branch_nodes_data, info.cull_data);
      it = sys->instances.erase(it);
    } else {
      ++it;
    }
  }
}

void process_events(RenderRootsSystem* sys, const UpdateInfo& info) {
  for (auto& [_, inst] : sys->instances) {
    auto roots_inst = read_roots_instance(info.roots_system, inst.associated_roots);
    if (roots_inst.events.grew) {
      inst.need_update_growth = true;
      assert(!roots_inst.events.receded);
    }
    if (roots_inst.events.receded || roots_inst.events.pruned) {
      inst.need_update_recede = true;
    }
    if (roots_inst.events.just_finished_pruning) {
      assert(roots_inst.events.pruned && inst.need_update_recede);
      inst.need_refill_receded = true;
    }
  }
}

bool update_growing_cull_bounds(
  RenderRootsInstance& inst, const TreeRootNode* nodes, int num_nodes, const UpdateInfo& info,
  bool fit_tight) {
  //
  auto& bounds_sets = inst.expanding_bounds_sets;
  if (fit_tight) {
    tightly_fit_bounds_sets(bounds_sets, nodes, num_nodes);
  } else {
    update_expanding_bounds_sets(bounds_sets, nodes, num_nodes);
  }

  //  Reserve AABBs
  for (int i = 0; i < bounds_sets.num_entries(); i++) {
    int group_ind = i / Config::cull_group_pool_size;
    int aabb_ind = i - group_ind * Config::cull_group_pool_size;
    if (group_ind >= int(inst.cull_groups.size())) {
      inst.cull_groups.emplace_back();
      inst.cull_groups.back() = cull::create_reserved_frustum_cull_instance_group(
        info.cull_data, uint32_t(Config::cull_group_pool_size));
    }
    if (bounds_sets.entries[i].modified) {
      const auto& bounds = bounds_sets.entries[i].bounds;
      auto& grp = inst.cull_groups[group_ind];
      cull::set_aabb(info.cull_data, grp, uint32_t(aabb_ind), bounds.min, bounds.max);
    }
  }

  //  Assign indices of culling AABBs to new root nodes
  bool any_modified{};
  for (int i = inst.num_growing_nodes_filled_cull_data; i < num_nodes; i++) {
    const int draw_pool_ind = i / Config::nodes_per_growing_drawable;
    const int draw_element = i - draw_pool_ind * Config::nodes_per_growing_drawable;

    assert(draw_pool_ind < int(inst.growing_drawable_component_pool.size()));
    auto& components = inst.growing_drawable_component_pool[draw_pool_ind];
    auto lod_data = tree::get_branch_nodes_lod_data(
      info.branch_nodes_data, components.base_drawable.value());

    assert(i < int(bounds_sets.nodes.size()));
    int cull_ind = bounds_sets.nodes[bounds_sets.nodes[i].set_root_index].ith_set;
    int group_ind = cull_ind / Config::cull_group_pool_size;
    int aabb_ind = cull_ind - group_ind * Config::cull_group_pool_size;
    assert(group_ind < int(inst.cull_groups.size()));

    auto cull_group_ind = inst.cull_groups[group_ind].group_index + 1;  //  @NOTE
    assert(cull_group_ind < 0xffffu && aabb_ind < int(0xffffu));

    auto& lod_element = lod_data[draw_element];
    lod_element.set_one_based_cull_group_and_zero_based_instance(
      uint16_t(cull_group_ind), uint16_t(aabb_ind));
    lod_element.set_is_active(true);

    tree::set_branch_nodes_lod_data_modified(
      info.branch_nodes_data, components.base_drawable.value());
    any_modified = true;
  }

  inst.num_growing_nodes_filled_cull_data = num_nodes;
  return any_modified;
}

void remake_cull_bounds(
  RenderRootsInstance& inst, const TreeRootNode* nodes, int num_nodes, const UpdateInfo& info) {
  //
  inst.num_growing_nodes_filled_cull_data = 0;
  inst.expanding_bounds_sets.clear();
  if (update_growing_cull_bounds(inst, nodes, num_nodes, info, true)) {
    //  @TODO -- remove this once second occlusion cull pass is implemented for branch nodes.
    //  `update_growing_cull_bounds` returns true if any LOD instances were modified. In that case,
    //  it's possible that an existing branch node was newly assigned a different frustum cull
    //  instance index compared to the previous frame, in which case the previous frame's cull result
    //  (culled vs not culled) might be incorrect for the new instance. This would be fine except
    //  that we haven't implemented the second occlusion culling pass yet (to check for disoccluded
    //  nodes), so there's an obvious 1-frame "pop" when the previous cull result is incorrect
    //  (and culled).
    tree::set_branch_nodes_lod_data_potentially_invalidated(info.branch_nodes_data);
  }
}

void update_growth(RenderRootsInstance& inst, const UpdateInfo& info) {
  if (!inst.need_update_growth) {
    return;
  }

  auto roots_inst = read_roots_instance(info.roots_system, inst.associated_roots);
  if (!roots_inst.roots) {
    return;
  }

  const int num_nodes = roots_inst.roots->curr_num_nodes;
  const TreeRootNode* nodes = roots_inst.roots->nodes.data();

  const float length_scale = roots_inst.roots->node_length_scale;
  const bool atten_radius_by_length = false;

  Temporary<TreeRootNodeFrame, 2048> store_frames;
  auto* node_frames = store_frames.require(num_nodes);
  assert(!store_frames.heap && "Alloc required.");
  compute_tree_root_node_frames(nodes, num_nodes, node_frames);

  int num_remaining = num_nodes;
  int drawable_component_index{};
  while (num_remaining > 0) {
    int node_offset = drawable_component_index * Config::nodes_per_growing_drawable;
    int next_offset = std::min(node_offset + Config::nodes_per_growing_drawable, num_nodes);
    int node_count = next_offset - node_offset;
    assert(node_count > 0);

    if (drawable_component_index >= int(inst.growing_drawable_component_pool.size())) {
      inst.growing_drawable_component_pool.emplace_back();
      auto& to_reserve = inst.growing_drawable_component_pool.back();
      to_reserve = create_reserved_tree_roots_drawable_components(
        info.branch_nodes_data, Config::nodes_per_growing_drawable);
    }

    auto& components = inst.growing_drawable_component_pool[drawable_component_index];

    fill_branch_nodes_instances_from_root_nodes(
      info.branch_nodes_data, components, nodes, node_frames, num_nodes, node_offset, node_count,
      length_scale, atten_radius_by_length);

    num_remaining -= node_count;
    drawable_component_index++;
  }

  update_growing_cull_bounds(inst, nodes, num_nodes, info, false);

  inst.need_update_growth = false;
}

void update_recede(RenderRootsInstance& inst, const UpdateInfo& info) {
  if (!inst.need_update_recede) {
    return;
  }

  auto roots_inst = read_roots_instance(info.roots_system, inst.associated_roots);
  if (!roots_inst.roots) {
    return;
  }

  const int num_nodes = roots_inst.roots->curr_num_nodes;
  const TreeRootNode* nodes = roots_inst.roots->nodes.data();

  const int node_capacity =
    int(inst.growing_drawable_component_pool.size()) * Config::nodes_per_growing_drawable;
  if (node_capacity < num_nodes) {
    //  Nodes "should" only be added during growth. This would not be hard to change, but assume
    //  true for now.
    assert(false);
    return;
  }

  const bool do_refill = inst.need_refill_receded;
  Temporary<TreeRootNodeFrame, 2048> store_frames;
  TreeRootNodeFrame* node_frames{};
  if (do_refill) {
    node_frames = store_frames.require(num_nodes);
    assert(!store_frames.heap && "Alloc required.");
    compute_tree_root_node_frames(nodes, num_nodes, node_frames);
    //  Recreate components
    for (auto& components : inst.growing_drawable_component_pool) {
      destroy_tree_roots_drawable_components(info.branch_nodes_data, &components);
      components = create_reserved_tree_roots_drawable_components(
        info.branch_nodes_data, Config::nodes_per_growing_drawable);
    }
  }

  const float length_scale = roots_inst.roots->node_length_scale;
  const bool atten_radius_by_length = true;

  int num_remaining = num_nodes;
  int drawable_component_index{};
  while (num_remaining > 0) {
    assert(drawable_component_index < inst.growing_drawable_component_pool.size());
    auto& components = inst.growing_drawable_component_pool[drawable_component_index];
    int node_offset = drawable_component_index * Config::nodes_per_growing_drawable;
    int next_offset = std::min(node_offset + Config::nodes_per_growing_drawable, num_nodes);
    int node_count = next_offset - node_offset;
    assert(node_count > 0);

    if (do_refill) {
      fill_branch_nodes_instances_from_root_nodes(
        info.branch_nodes_data, components, nodes, node_frames, num_nodes, node_offset, node_count,
        length_scale, atten_radius_by_length);
    } else {
      set_position_and_radii_from_root_nodes(
        info.branch_nodes_data, components, nodes,
        num_nodes, node_offset, node_count, length_scale, atten_radius_by_length);
    }

    num_remaining -= node_count;
    drawable_component_index++;
  }

  if (do_refill) {
    remake_cull_bounds(inst, nodes, num_nodes, info);
  }

  inst.need_update_recede = false;
  inst.need_refill_receded = false;
}

} //  anon

RenderRootsInstanceHandle
tree::create_render_roots_instance(RenderRootsSystem* sys,
                                   const CreateRenderRootsInstanceParams& params) {
  assert(params.associated_roots.is_valid());
  RenderRootsInstanceHandle result{sys->next_instance_id++};
  sys->instances[result.id] = make_instance(params);
  return result;
}

void tree::destroy_render_roots_instance(RenderRootsSystem* sys, RenderRootsInstanceHandle handle) {
  if (auto it = sys->instances.find(handle.id); it != sys->instances.end()) {
    it->second.need_destroy = true;
  } else {
    assert(false);
  }
}

RenderRootsSystem* tree::create_render_roots_system() {
  return new RenderRootsSystem();
}

void tree::update_render_roots_system(RenderRootsSystem* sys, const UpdateInfo& info) {
  destroy_pending(sys, info);
  process_events(sys, info);

  for (auto& [_, inst] : sys->instances) {
    update_growth(inst, info);
    update_recede(inst, info);
  }
}

void tree::destroy_render_roots_system(RenderRootsSystem** sys) {
  delete *sys;
  *sys = nullptr;
}

GROVE_NAMESPACE_END
