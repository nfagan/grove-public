#pragma once

#include "grove/math/Bounds3.hpp"
#include "grove/common/Optional.hpp"

namespace grove {
class AudioComponent;
class SimpleAudioNodePlacement;
class AudioPortPlacement;
class Terrain;
struct PitchSampleSetGroupHandle;
}

namespace grove::tree {

struct ResourceSpiralAroundNodesSystem;

struct ResourceFlowAlongNodesInstrumentUpdateResult {
  Optional<Bounds3f> insert_node_bounds_into_accel;
  bool* acknowledge_inserted;
};

ResourceFlowAlongNodesInstrumentUpdateResult update_resource_flow_along_nodes_instrument(
  ResourceSpiralAroundNodesSystem* sys, AudioComponent& component,
  SimpleAudioNodePlacement& node_placement, AudioPortPlacement& port_placement,
  const PitchSampleSetGroupHandle& pitch_sample_set_group,
  const Terrain& terrain, double real_dt);

}