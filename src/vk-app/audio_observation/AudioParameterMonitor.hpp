#pragma once

#include "../audio_core/AudioNodeStorage.hpp"
#include "../audio_core/UIAudioParameterManager.hpp"
#include <unordered_map>

namespace grove::observe {

class AudioParameterMonitor {
public:
  using ValueCallback = std::function<void(const AudioParameterDescriptor&, const UIAudioParameter&)>;

  enum class CallbackMethod {
    OnUpdate,   //  callback whenever a new value is received from the audio thread.
    OnChange,   //  callback only if a new value is received from the audio thread and this value
                //    is different from the last value.
    Always      //  always callback every frame.
  };

  struct MonitorableParameter {
    const char* name{""};
    AudioParameterIDs ids{};
    ValueCallback callback{};
    AudioParameterValue last_value{};
    CallbackMethod callback_method{};
  };

  struct MonitorableNode {
    DynamicArray<MonitorableParameter, 2> params;
  };

public:
  void add_node(AudioNodeStorage::NodeID id, MonitorableNode&& node);
  void remove_node(AudioNodeStorage::NodeID by_id,
                   UIAudioParameterManager& parameter_manager);
  void update(UIAudioParameterManager& parameter_manager,
              const AudioNodeStorage& node_storage);

  int num_nodes() const {
    return int(nodes.size());
  }

  static MonitorableParameter make_pending_monitorable_parameter(const char* name,
                                                                 ValueCallback&& callback,
                                                                 CallbackMethod method);

private:
  std::unordered_map<AudioNodeStorage::NodeID, MonitorableNode> nodes;
};

}