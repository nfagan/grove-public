#pragma once

#include "../audio_core/AudioNodeStorage.hpp"

namespace grove {

class AudioBufferStore;
struct AudioBufferHandle;
struct AudioParameterSystem;
struct UIAudioParameter;
class AudioScale;

class ProceduralTreeInstrument {
public:
  struct Instance {
    std::function<void(const AudioParameterDescriptor&, const UIAudioParameter&)> callback{};
  };

  struct ObservableChange {
    AudioNodeStorage::NodeID node_id{};
    AudioParameterID parameter_id{};
    float value{};
  };

  using ObservableChanges = DynamicArray<ObservableChange, 4>;
public:
  Instance create_instance(AudioNodeStorage::NodeID id);
  ObservableChanges update();

private:
  ObservableChanges changes;
};

}