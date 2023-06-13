#pragma once

#include "roots_components.hpp"

namespace grove::tree {

int prune_rejected_axes(const TreeRootNode* src, const bool* accepted,
                        int num_src, TreeRootNodeIndices* dst, int* dst_to_src);

void copy_nodes_applying_node_indices(const TreeRootNode* src, const int* dst_to_src,
                                      const TreeRootNodeIndices* node_indices,
                                      int num_dst, TreeRootNode* dst);

}