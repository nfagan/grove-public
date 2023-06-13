#include "debug_node_rendering.hpp"
#include "../procedural_tree/ProceduralTreeComponent.hpp"
#include "tree_system.hpp"
#include "roots_system.hpp"
#include "fit_growing_root_bounds.hpp"
#include "fit_bounds.hpp"
#include "../render/debug_draw.hpp"
#include "render.hpp"
#include "grove/common/common.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/bounds.hpp"
#include "grove/math/Frustum.hpp"
#include "grove/common/Stopwatch.hpp"
#include <imgui/imgui.h>

/*
 * Roots: reserve aabbs according to axis roots
 */

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateInfo = tree::debug::NodeRenderingUpdateInfo;
using namespace tree;

struct BoundsEntry {
  Bounds3f bounds;
  int node0{};
  int num_nodes{};
};

template <typename T>
struct GetOBB3 {};

template <>
struct GetOBB3<tree::TreeRootNode> {
  static OBB3f get(const tree::TreeRootNode& node, float r_scale) {
    return make_tree_root_node_obb(
      node.position, node.direction, node.target_length, node.target_diameter * r_scale);
  }
};

template <>
struct GetOBB3<tree::Internode> {
  static OBB3f get(const tree::Internode& node, float r_scale) {
    return internode_obb_custom_diameter(node, node.diameter * r_scale);
  }
};

template <typename Node>
BoundsEntry fit_axis(const Node* nodes, int* src, int n, bool do_fit, float r_scale) {
  assert(*src != -1);
  BoundsEntry result{};
  result.node0 = *src;
  int ct{};
  while (*src != -1) {
    auto& node = nodes[*src];
    if (do_fit) {
      result.bounds = union_of(result.bounds, obb3_to_aabb(GetOBB3<Node>::get(node, r_scale)));
    }
    *src = node.medial_child;
    if (++ct == n) {
      break;
    }
  }
  result.num_nodes = ct;
  return result;
}

template <typename Node>
int fit_axes(const Node* nodes, int num_nodes, int interval, BoundsEntry* entries, bool do_fit,
             float r_scale) {
  if (num_nodes == 0) {
    return 0;
  }

  Temporary<int, 2048> store_stack;
  auto* stack = store_stack.require(num_nodes);
  int si{};
  stack[si++] = 0;

  int n{};
  while (si > 0) {
    int ni = stack[--si];
    int ar = ni;
    while (ar != -1) {
      if (nodes[ar].has_lateral_child()) {
        stack[si++] = nodes[ar].lateral_child;
      }
      ar = nodes[ar].medial_child;
    }
    while (ni != -1) {
      entries[n++] = fit_axis(nodes, &ni, interval, do_fit, r_scale);
    }
  }

  return n;
}

struct BoundsSetInstances {
  std::unordered_map<uint32_t, ExpandingBoundsSets> instances;
};

void draw_bounds(const BoundsSetInstances& instances) {
  for (auto& [_, inst] : instances.instances) {
    for (int i = 0; i < int(inst.nodes.size()); i++) {
      if (inst.nodes[i].set_root_index == i) {
        auto& bounds = inst.entries[inst.nodes[i].ith_set].bounds;
        vk::debug::draw_aabb3(bounds, Vec3f{1.0f, 0.0f, 0.0f});
      }
    }
  }
}

auto update_bounds_set_instances(
  BoundsSetInstances& insts, const tree::RootsInstanceHandle* roots_handles, int num_roots,
  const debug::NodeRenderingUpdateInfo& info) {
  //
  struct Result {
    float elapsed_time_ms;
  };
  Result result{};

  for (int i = 0; i < num_roots; i++) {
    auto read_roots = tree::read_roots_instance(info.roots_sys, roots_handles[i]);
    if (!read_roots.roots) {
      continue;
    }
    if (insts.instances.count(roots_handles[i].id) == 0) {
      insts.instances[roots_handles[i].id] = {};
    }
  }

  Stopwatch t0;
  for (auto& [id, inst] : insts.instances) {
    auto read_roots = tree::read_roots_instance(info.roots_sys, tree::RootsInstanceHandle{id});
    if (read_roots.roots) {
      update_expanding_bounds_sets(
        inst, read_roots.roots->nodes.data(), read_roots.roots->curr_num_nodes);
    }
  }
  result.elapsed_time_ms = float(t0.delta().count() * 1e3);
  return result;
}

struct {
  BoundsSetInstances bounds_set_instances;
  std::vector<BoundsEntry> root_bounds_entries;
  std::vector<BoundsEntry> tree_bounds_entries;
  int num_root_nodes{};
  int num_tree_nodes{};
  int bounds_interval{4};
  int min_medial{4};
  int max_medial{4};
  bool enabled{};
  bool fit_disabled{};
  bool orig_fit_disabled{};
  bool use_fit2{true};
  bool disable_fit{};
  float fit2_xz_thresh{2.0f};
  bool draw_bounds{};
  bool draw_bounds_set_bounds{};
  float root_time_ms{};
  float tree_time_ms{};
  float bounds_set_time_ms{};
  bool do_frustum_cull{};
  int num_tree_culled{};
} globals;

} //  anon

void tree::debug::update_fit_node_aabbs(const NodeRenderingUpdateInfo& info) {
  if (!globals.enabled) {
    return;
  }

  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees) {
    return;
  }

  auto& tot_root_entries = globals.root_bounds_entries;
  auto& tot_tree_entries = globals.tree_bounds_entries;
  tot_root_entries.clear();
  tot_tree_entries.clear();

  const int bounds_interval = globals.bounds_interval;
  globals.num_root_nodes = 0;
  globals.num_tree_nodes = 0;

  Stopwatch stopwatch;

  tree::RootsInstanceHandle roots_handles[2048];
  int num_roots = tree::collect_roots_instance_handles(info.roots_sys, roots_handles, 2048);
  for (int i = 0; i < num_roots; i++) {
    if (globals.orig_fit_disabled) {
      continue;
    }

    auto read_roots = tree::read_roots_instance(info.roots_sys, roots_handles[i]);
    if (!read_roots.roots) {
      continue;
    }

    auto* node_beg = read_roots.roots->nodes.data();
    const int num_nodes = read_roots.roots->curr_num_nodes;
    Temporary<BoundsEntry, 2048> store_entries;
    auto* entries = store_entries.require(num_nodes);
    int num_gen = fit_axes(node_beg, num_nodes, bounds_interval, entries, !globals.fit_disabled, 1.0f);

    if (globals.draw_bounds) {
      for (int j = 0; j < num_gen; j++) {
        vk::debug::draw_aabb3(entries[j].bounds, Vec3f{0.0f, 1.0f, 0.0f});
      }
    }

    tot_root_entries.insert(tot_root_entries.end(), entries, entries + num_gen);
    globals.num_root_nodes += num_nodes;
  }

  globals.root_time_ms = float(stopwatch.delta().count() * 1e3);
  stopwatch.reset();

  for (auto& [_, tree] : *trees) {
    if (globals.orig_fit_disabled) {
      continue;
    }

    auto inst = tree::read_tree(info.tree_sys, tree.instance);
    if (!inst.nodes) {
      continue;
    }

    auto& inodes = inst.nodes->internodes;
    int num_nodes = int(inodes.size());
    auto* node_beg = inodes.data();

    if (globals.disable_fit) {
      for (int i = 0; i < num_nodes; i++) {
        auto& entry = tot_tree_entries.emplace_back();
        entry.bounds = obb3_to_aabb(internode_obb(node_beg[i]));
      }
    } else {
      Temporary<Bounds3f, 2048> store_bounds;
      auto* bounds = store_bounds.require(num_nodes);

      Temporary<int, 2048> store_assigned_indices;
      int* assigned_indices = store_assigned_indices.require(num_nodes);

      int num_gen{};
      if (globals.use_fit2) {
        Temporary<Mat3f, 2048> store_node_frames;
        Mat3f* node_frames = store_node_frames.require(num_nodes);
        compute_internode_frames(node_beg, num_nodes, node_frames);

        num_gen = bounds::fit_aabbs_around_axes_radius_threshold_method(
          node_beg, node_frames, num_nodes,
          globals.min_medial, globals.max_medial, globals.fit2_xz_thresh,
          bounds, assigned_indices);

      } else {
        num_gen = bounds::fit_aabbs_around_axes_only_medial_children_method(
          node_beg, num_nodes, bounds_interval, bounds, assigned_indices);
      }

      for (int i = 0; i < num_gen; i++) {
        auto& entry = tot_tree_entries.emplace_back();
        entry.bounds = bounds[i];
      }
    }

    globals.num_tree_nodes += num_nodes;
  }

  globals.tree_time_ms = float(stopwatch.delta().count() * 1e3);

  if (globals.draw_bounds) {
    for (auto& entry : tot_tree_entries) {
      vk::debug::draw_aabb3(entry.bounds, Vec3f{0.0f, 1.0f, 0.0f});
    }
  }

  {
    auto res = update_bounds_set_instances(
      globals.bounds_set_instances, roots_handles, num_roots, info);
    globals.bounds_set_time_ms = res.elapsed_time_ms;
  }

  if (globals.draw_bounds_set_bounds) {
    draw_bounds(globals.bounds_set_instances);
  }

  globals.num_tree_culled = 0;
  if (globals.do_frustum_cull) {
    auto frust = info.camera.make_world_space_frustum(512.0f);
    for (auto& entry : tot_tree_entries) {
      if (!frustum_aabb_intersect(frust, entry.bounds)) {
        globals.num_tree_culled++;
      }
    }
  }
}

void tree::debug::render_fit_node_aabbs_gui_dropdown() {
  const int num_tree_bounds =int(globals.tree_bounds_entries.size());

  ImGui::Text("Num tree culled: %d", globals.num_tree_culled);
  ImGui::Text("P tree visible: %0.3f",
              float(num_tree_bounds - globals.num_tree_culled) / float(num_tree_bounds));

  ImGui::Text("Num root nodes: %d", globals.num_root_nodes);
  ImGui::Text("Num root bounds entries: %d", int(globals.root_bounds_entries.size()));
  ImGui::Text("Desired root frac: %0.3f, actual: %0.3f",
              1.0f / float(globals.bounds_interval),
              float(globals.root_bounds_entries.size()) / float(globals.num_root_nodes));
  ImGui::Text("Num tree nodes: %d", globals.num_tree_nodes);
  ImGui::Text("Num tree bounds entries: %d", int(globals.tree_bounds_entries.size()));
  ImGui::Text("Desired tree frac: %0.3f, actual: %0.3f",
              1.0f / float(globals.bounds_interval),
              float(globals.tree_bounds_entries.size()) / float(globals.num_tree_nodes));
  ImGui::Text("Root ms: %0.3f", globals.root_time_ms);
  ImGui::Text("Tree ms: %0.3f", globals.tree_time_ms);
  ImGui::Text("Bounds set ms: %0.3f", globals.bounds_set_time_ms);

  ImGui::Checkbox("Enabled", &globals.enabled);
  ImGui::Checkbox("FitDisabled", &globals.fit_disabled);
  ImGui::Checkbox("OrigFitDisabled", &globals.orig_fit_disabled);
  ImGui::Checkbox("DrawBounds", &globals.draw_bounds);
  ImGui::Checkbox("DrawBoundsSetBounds", &globals.draw_bounds_set_bounds);

  ImGui::Checkbox("DisableFit", &globals.disable_fit);
  ImGui::Checkbox("DoFrustumCull", &globals.do_frustum_cull);
  ImGui::Checkbox("UseFit2", &globals.use_fit2);
  ImGui::SliderFloat("Fit2XZThreshold", &globals.fit2_xz_thresh, 0.0f, 8.0f);

  if (ImGui::InputInt("BoundsInterval", &globals.bounds_interval)) {
    globals.bounds_interval = std::max(1, globals.bounds_interval);
  }

  if (ImGui::InputInt("MinMedial", &globals.min_medial)) {
    globals.min_medial = std::max(1, globals.min_medial);
  }
  if (ImGui::InputInt("MaxMedial", &globals.max_medial)) {
    globals.max_medial = std::max(1, globals.max_medial);
  }
}

GROVE_NAMESPACE_END

