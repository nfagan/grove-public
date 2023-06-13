#pragma once

#include "AudioConnectionManager.hpp"
#include "audio_port_placement.hpp"
#include "../cabling/CablePathFinder.hpp"
#include "grove/common/DynamicArray.hpp"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace grove {

class CablePathFinder;

struct UIAudioConnectionManager {
public:
  struct UpdateInfo {
    const AudioNodeStorage* node_storage;
    AudioConnectionManager* connection_manager;
    const AudioPortPlacement* port_placement;
    CablePathFinder* cable_path_finder;
    const SelectedInstrumentComponents* selected_components;
    ArrayView<AudioConnectionManager::Connection> new_connections;
    ArrayView<AudioConnectionManager::Connection> new_disconnections;
  };

  struct UpdateResult {
    ArrayView<CablePath> new_cable_paths;
    ArrayView<uint32_t> cable_paths_to_remove;
    ArrayView<uint32_t> selected_cable_paths;
    bool had_connection_failure{};
    bool did_connect{};
    bool did_disconnect{};
  };

  struct UpdateState {
    void clear() {
      attempt_to_connect = false;
      attempt_to_disconnect = NullOpt{};
    }

    bool attempt_to_connect = false;
    Optional<AudioNodeStorage::PortID> attempt_to_disconnect;
  };

public:
  UpdateResult update(const UpdateInfo& update_info);
  void attempt_to_connect();
  void attempt_to_disconnect(AudioNodeStorage::PortID id);

public:
  using CableConnectionMap =
    std::unordered_map<AudioConnectionManager::Connection, uint32_t,
                       AudioConnectionManager::HashConnection,
                       AudioConnectionManager::EqConnectionPortOrderIndependent>;

  std::vector<CablePath> new_cable_paths;
  DynamicArray<uint32_t, 2> cable_paths_to_remove;
  DynamicArray<uint32_t, 4> selected_cable_paths;

  uint32_t next_cable_path_id{0};
  CableConnectionMap connections_to_cable_paths;

  UpdateState update_state;
};

}