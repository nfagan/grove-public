#pragma once

#include "../audio_core/AudioNodeStorage.hpp"
#include "../audio_observation/Bender.hpp"

namespace grove {

class ProceduralFlowerBenderInstrument {
public:
  struct ObservableChange {
    AudioNodeStorage::NodeID id{};
    float value{};
  };

  struct UpdateResult {
    bool spawn_particle{};
  };

  struct Instance {
    float signal_value{};
  };

  using ObservableChanges = DynamicArray<ObservableChange, 4>;

public:
  observe::AudioParameterMonitor::MonitorableNode create_instance(AudioNodeStorage::NodeID id);
  UpdateResult update();

private:
  ObservableChanges quantization_changes;
  ObservableChanges signal_changes;
  std::unordered_map<AudioNodeStorage::NodeID, Instance> instances;
};

}