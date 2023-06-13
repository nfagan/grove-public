#pragma once

#include "components.hpp"

namespace grove::tree {

void apply_environment_input(ArrayView<Bud> buds,
                             const ArrayView<Internode>& internodes,
                             TreeNodeIndex root_inode_index,
                             const EnvironmentInputs& inputs,
                             const DistributeBudQParams& params);

void apply_environment_input(TreeNodeStore& tree_nodes,
                             const EnvironmentInputs& inputs,
                             const DistributeBudQParams& params);

}