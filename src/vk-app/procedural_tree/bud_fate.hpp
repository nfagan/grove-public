#pragma once

#include "components.hpp"
#include <limits>
#include <functional>

namespace grove::tree {

DynamicArray<int, 4> internode_bud_fate(TreeNodeIndex internode_index,
                                        TreeNodeStore& tree_nodes,
                                        const EnvironmentInputs& inputs,
                                        const SpawnInternodeParams& params);

void remove_grown_buds(TreeNodeStore& tree_nodes, const ArrayView<int>& remove_at);

void bud_fate(TreeNodeStore& tree_nodes,
              const EnvironmentInputs& inputs,
              const SpawnInternodeParams& params);

void set_diameter(Internodes& internodes,
                  const SpawnInternodeParams& params,
                  TreeNodeIndex root_index = 0);
void set_diameter(Internode* internodes, int num_internodes, const SpawnInternodeParams& params,
                  TreeNodeIndex root_index = 0);

}