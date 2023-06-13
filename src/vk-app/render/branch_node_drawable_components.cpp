#include "branch_node_drawable_components.hpp"
#include "render_branch_nodes_types.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/utility.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

[[maybe_unused]] constexpr const char* logging_id() {
  return "branch_node_drawable_components";
}

void set_dynamic_data(ArrayView<RenderBranchNodeDynamicData>& dyn_data, const Internodes& inodes) {
  const auto num_nodes = uint32_t(inodes.size());
  assert(num_nodes == dyn_data.size());
  for (uint32_t i = 0; i < num_nodes; i++) {
    auto& src_node = inodes[i];

    int child{-1};
    if (src_node.has_medial_child()) {
      child = src_node.medial_child;
    } else if (src_node.has_lateral_child()) {
      child = src_node.lateral_child;
    }

    Vec3f child_p{};
    float child_r{};
    if (child == -1) {
      child_p = src_node.render_tip_position();
      child_r = 0.0f;
    } else {
      child_p = inodes[child].render_position;
      child_r = inodes[child].radius();
    }

    dyn_data[i].set_position_and_radii(src_node.render_position, src_node.radius(), child_p, child_r);
  }
}

} //  anon

BranchNodeDrawableComponents
tree::create_wind_branch_node_drawable_components_from_internodes(
  RenderBranchNodesData* data, const Internodes& inodes,
  const Bounds3f& eval_aabb, const AxisRootInfo& axis_roots,
  const RemappedAxisRoots& remapped_roots) {
  //
  Temporary<RenderBranchNodeInstanceDescriptor, 2048> store_instance_descs;
  auto* instance_descs = store_instance_descs.require(int(inodes.size()));
  assert(!store_instance_descs.heap && "Alloc required.");

  Temporary<Mat3f, 2048> store_frames;
  auto* frames = store_frames.require(int(inodes.size()));
  compute_internode_frames(inodes.data(), int(inodes.size()), frames);

  const auto num_nodes = uint32_t(inodes.size());
  for (uint32_t i = 0; i < num_nodes; i++) {
    auto& src_node = inodes[i];

    int child{-1};
    if (src_node.has_medial_child()) {
      child = src_node.medial_child;
    } else if (src_node.has_lateral_child()) {
      child = src_node.lateral_child;
    }

    auto self_wind_info = make_wind_axis_root_info(
      src_node, inodes, axis_roots, remapped_roots, eval_aabb);
    auto child_wind_info = child == -1 ? self_wind_info : make_wind_axis_root_info(
      inodes[child], inodes, axis_roots, remapped_roots, eval_aabb);
    auto packed_wind_info = to_packed_wind_info(self_wind_info, child_wind_info);

    Vec3f child_p{};
    float child_r{};
    uint32_t child_fi = i;  //  child frame index, evaluate as self if no child.
    if (child == -1) {
      child_p = src_node.render_tip_position();
      child_r = 0.0f;
    } else {
      child_p = inodes[child].render_position;
      child_r = inodes[child].radius();
      child_fi = uint32_t(child);
    }

    RenderBranchNodeInstanceDescriptor desc{};
    desc.frustum_cull_instance_group = 0; //  @TODO
    desc.frustum_cull_instance = 0; //  @TODO
    desc.self_position = src_node.render_position;
    desc.self_radius = src_node.radius();
    desc.child_position = child_p;
    desc.child_radius = child_r;
    desc.self_right = frames[i][0];
    desc.self_up = frames[i][1];
    desc.child_right = frames[child_fi][0];
    desc.child_up = frames[child_fi][1];
    desc.wind_info0 = packed_wind_info[0];
    desc.wind_info1 = packed_wind_info[1];
    desc.wind_info2 = packed_wind_info[2];
    instance_descs[i] = desc;
  }

  RenderBranchNodeAggregateDescriptor aggregate_desc{};
  aggregate_desc.aabb_p0 = eval_aabb.min;
  aggregate_desc.aabb_p1 = eval_aabb.max;

  auto wind_handle = create_wind_branch_node_drawable(
    data, instance_descs, num_nodes, aggregate_desc);

  BranchNodeDrawableComponents result;
  result.wind_drawable = wind_handle;
  return result;
}

void tree::set_position_and_radii_from_internodes(RenderBranchNodesData* data,
                                                  const BranchNodeDrawableComponents& components,
                                                  const Internodes& inodes) {
  if (components.wind_drawable) {
    ArrayView<RenderBranchNodeDynamicData> dyn_data = get_branch_nodes_dynamic_data(
      data, components.wind_drawable.value());

    if (inodes.size() == size_t(dyn_data.size())) {
      set_dynamic_data(dyn_data, inodes);
      set_branch_nodes_dynamic_data_modified(data, components.wind_drawable.value());
    } else {
      GROVE_LOG_ERROR_CAPTURE_META(
        "Number of internodes != number of dynamic wind instances.", logging_id());
    }
  }

  if (components.base_drawable) {
    ArrayView<RenderBranchNodeDynamicData> dyn_data = get_branch_nodes_dynamic_data(
      data, components.base_drawable.value());

    if (inodes.size() == size_t(dyn_data.size())) {
      set_dynamic_data(dyn_data, inodes);
      set_branch_nodes_dynamic_data_modified(data, components.base_drawable.value());
    } else {
      GROVE_LOG_ERROR_CAPTURE_META(
        "Number of internodes != number of dynamic base instances.", logging_id());
    }
  }
}

void tree::destroy_branch_node_drawable_components(RenderBranchNodesData* data,
                                                   BranchNodeDrawableComponents* components) {
  if (components->base_drawable) {
    destroy_branch_node_drawable(data, components->base_drawable.value());
    components->base_drawable = NullOpt{};
  }
  if (components->wind_drawable) {
    destroy_wind_branch_node_drawable(data, components->wind_drawable.value());
    components->wind_drawable = NullOpt{};
  }
}

GROVE_NAMESPACE_END
