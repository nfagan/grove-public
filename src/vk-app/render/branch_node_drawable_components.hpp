#pragma once

#include "render_branch_nodes.hpp"
#include "../procedural_tree/components.hpp"
#include "grove/common/Optional.hpp"

namespace grove::tree {

struct RemappedAxisRoots;

struct BranchNodeDrawableComponents {
  Optional<BranchNodeDrawableHandle> base_drawable;
  Optional<WindBranchNodeDrawableHandle> wind_drawable;
};

BranchNodeDrawableComponents
create_wind_branch_node_drawable_components_from_internodes(
  RenderBranchNodesData* data, const Internodes& inodes,
  const Bounds3f& eval_aabb, const AxisRootInfo& axis_root_info,
  const RemappedAxisRoots& remapped_axis_roots);

void set_position_and_radii_from_internodes(RenderBranchNodesData* data,
                                            const BranchNodeDrawableComponents& components,
                                            const Internodes& inodes);

void destroy_branch_node_drawable_components(RenderBranchNodesData* data,
                                             BranchNodeDrawableComponents* components);

}