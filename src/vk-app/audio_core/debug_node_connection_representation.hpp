#pragma once

#include "AudioConnectionManager.hpp"

namespace grove {
class AudioPortPlacement;
class SelectedInstrumentComponents;
}

namespace grove::tree {
struct ResourceSpiralAroundNodesSystem;
}

namespace grove::audio {
struct NodeSignalValueSystem;
}

namespace grove::audio::debug {

struct NodeConnectionReprUpdateInfo {
  const AudioPortPlacement& port_placement;
  const SelectedInstrumentComponents& selected;
  tree::ResourceSpiralAroundNodesSystem* resource_spiral_sys;
  const AudioNodeStorage& node_storage;
  const audio::NodeSignalValueSystem* node_signal_value_system;
  const AudioConnectionManager::UpdateResult& connect_update_res;
};

void update_node_connection_representation(const NodeConnectionReprUpdateInfo& info);

}