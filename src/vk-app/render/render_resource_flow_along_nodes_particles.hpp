#pragma once

namespace grove::tree {
struct SpiralAroundNodesUpdateContext;
}

namespace grove::particle {

void push_resource_flow_along_nodes_particles(
  const tree::SpiralAroundNodesUpdateContext* contexts, int num_contexts);

}