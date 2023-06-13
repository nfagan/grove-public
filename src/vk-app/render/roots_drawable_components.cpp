#include "roots_drawable_components.hpp"
#include "render_branch_nodes_types.hpp"
#include "../procedural_tree/roots_render.hpp"
#include "grove/common/common.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

constexpr float tip_radius() {
  return 0.0025f;
}

[[maybe_unused]] constexpr const char* logging_id() {
  return "roots_drawable_components";
}

const TreeRootNode* child_of(const TreeRootNode& node, const TreeRootNode* nodes) {
  if (node.has_medial_child()) {
    return nodes + node.medial_child;
  } else if (node.has_lateral_child()) {
    return nodes + node.lateral_child;
  } else {
    return nullptr;
  }
}

} //  anon

TreeRootsDrawableComponents
tree::create_reserved_tree_roots_drawable_components(RenderBranchNodesData* data, int num_nodes) {
  Temporary<RenderBranchNodeInstanceDescriptor, 2048> store_descs;
  auto* descs = store_descs.require(num_nodes);
  assert(!store_descs.heap && "Alloc required.");

  for (int i = 0; i < num_nodes; i++) {
    //  Valid frame to avoid NaNs, but radii and position == 0 such that nodes are not visible.
    descs[i] = {};
    descs[i].self_right = ConstVec3f::positive_x;
    descs[i].self_up = ConstVec3f::positive_y;
    descs[i].child_right = ConstVec3f::positive_x;
    descs[i].child_up = ConstVec3f::positive_y;
  }

  RenderBranchNodeAggregateDescriptor placeholder_desc{};
  placeholder_desc.aabb_p1 = Vec3f{1.0f};

  TreeRootsDrawableComponents result;
  result.base_drawable = create_branch_node_drawable(data, descs, num_nodes, placeholder_desc);
  return result;
}

void tree::fill_branch_nodes_instances_from_root_nodes(
  RenderBranchNodesData* data,
  const TreeRootsDrawableComponents& components,
  const TreeRootNode* all_nodes, const TreeRootNodeFrame* all_node_frames,
  int num_nodes, int node_offset, int node_count,
  float length_scale, bool atten_radius_by_length) {
  //
  assert(node_count <= num_nodes);
  (void) num_nodes;
  if (!components.base_drawable) {
    return;
  }

  ArrayView<RenderBranchNodeStaticData> static_data =
    get_branch_nodes_static_data(data, components.base_drawable.value());
  ArrayView<RenderBranchNodeDynamicData> dyn_data =
    get_branch_nodes_dynamic_data(data, components.base_drawable.value());

  assert(static_data.size() == dyn_data.size());
  if (int(static_data.size()) < node_count) {
    GROVE_LOG_ERROR_CAPTURE_META(
      "Number of root nodes is greater than the number of reserved base instances.", logging_id());
    return;
  }

  for (int i = 0; i < node_count; i++) {
    const auto& node = all_nodes[i + node_offset];
    const auto& node_frame = all_node_frames[i + node_offset];

    auto& static_inst = static_data[i];
    auto& dyn_inst = dyn_data[i];

    const auto& self_right = node_frame.i;
    const auto& self_up = node_frame.j;

    Vec3f self_position = node.position;
    float self_radius = node.diameter * 0.5f;

    if (atten_radius_by_length) {
      self_radius *= (node.length / length_scale);
    }

    Vec3f child_right;
    Vec3f child_up;
    Vec3f child_position;
    float child_radius;

    if (auto* child = child_of(node, all_nodes)) {
      child_position = child->position;
      child_radius = child->diameter * 0.5f;

      if (atten_radius_by_length) {
        child_radius *= (child->length / length_scale);
      }

      int child_ind = int(child - all_nodes);
      child_right = all_node_frames[child_ind].i;
      child_up = all_node_frames[child_ind].j;
    } else {
      child_position = node.tip_position();
      child_radius = tip_radius();
      if (atten_radius_by_length) {
        child_radius *= (node.length / length_scale);
      }

      child_right = self_right;
      child_up = self_up;
    }

    dyn_inst.set_position_and_radii(self_position, self_radius, child_position, child_radius);
    static_inst.set_directions(self_right, self_up, child_right, child_up);
  }

  set_branch_nodes_dynamic_data_modified(data, components.base_drawable.value());
  set_branch_nodes_static_data_modified(data, components.base_drawable.value());
}

void tree::set_position_and_radii_from_root_nodes(
  RenderBranchNodesData* data, const TreeRootsDrawableComponents& components,
  const TreeRootNode* all_nodes, int num_nodes, int node_offset, int node_count,
  float length_scale, bool atten_radius_by_length) {
  //
  assert(node_count <= num_nodes);
  (void) num_nodes;
  if (!components.base_drawable) {
    return;
  }

  ArrayView<RenderBranchNodeDynamicData> dyn_data =
    get_branch_nodes_dynamic_data(data, components.base_drawable.value());

  if (int(dyn_data.size()) < node_count) {
    GROVE_LOG_ERROR_CAPTURE_META(
      "Number of root nodes is greater than the number of reserved base instances.", logging_id());
    return;
  }

  for (int i = 0; i < node_count; i++) {
    const auto& node = all_nodes[i + node_offset];
    auto& dyn_inst = dyn_data[i];

    Vec3f self_position = node.position;
    float self_radius = node.diameter * 0.5f;
    if (atten_radius_by_length) {
      self_radius *= (node.length / length_scale);
    }

    Vec3f child_position;
    float child_radius;
    if (auto* child = child_of(node, all_nodes)) {
      child_position = child->position;
      child_radius = child->diameter * 0.5f;
      if (atten_radius_by_length) {
        child_radius *= (child->length / length_scale);
      }
    } else {
      child_position = node.tip_position();
      child_radius = tip_radius();
      if (atten_radius_by_length) {
        child_radius *= (node.length / length_scale);
      }
    }

    dyn_inst.set_position_and_radii(self_position, self_radius, child_position, child_radius);
  }

  set_branch_nodes_dynamic_data_modified(data, components.base_drawable.value());
}

void tree::destroy_tree_roots_drawable_components(RenderBranchNodesData* data,
                                                  TreeRootsDrawableComponents* components) {
  if (components->base_drawable) {
    destroy_branch_node_drawable(data, components->base_drawable.value());
    components->base_drawable = NullOpt{};
  }
}

GROVE_NAMESPACE_END
