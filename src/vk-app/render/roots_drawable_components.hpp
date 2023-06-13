#pragma once

#include "render_branch_nodes.hpp"
#include "../procedural_tree/roots_components.hpp"
#include "grove/common/Optional.hpp"

namespace grove::tree {

struct TreeRootNodeFrame;

struct TreeRootsDrawableComponents {
  Optional<BranchNodeDrawableHandle> base_drawable;
};

TreeRootsDrawableComponents
create_reserved_tree_roots_drawable_components(RenderBranchNodesData* data, int num_reserve);

void fill_branch_nodes_instances_from_root_nodes(RenderBranchNodesData* data,
                                                 const TreeRootsDrawableComponents& components,
                                                 const TreeRootNode* all_nodes,
                                                 const TreeRootNodeFrame* all_node_frames,
                                                 int num_nodes, int node_offset, int node_count,
                                                 float length_scale, bool atten_radius_by_length);

void set_position_and_radii_from_root_nodes(RenderBranchNodesData* data,
                                            const TreeRootsDrawableComponents& components,
                                            const TreeRootNode* all_nodes,
                                            int num_nodes, int node_offset, int node_count,
                                            float length_scale, bool atten_radius_by_length);

void destroy_tree_roots_drawable_components(RenderBranchNodesData* data,
                                            TreeRootsDrawableComponents* components);

}