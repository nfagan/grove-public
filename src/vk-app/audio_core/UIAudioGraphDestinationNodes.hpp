#pragma once

#include "AudioNodeStorage.hpp"
#include "grove/audio/AudioParameterWriteAccess.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove {

class DestinationNode;
class AudioGraphRenderer;
class AudioRecorder;
struct AudioRecordStreamHandle;
class UIAudioParameterManager;
struct AudioParameterSystem;

class UIAudioGraphDestinationNodes {
public:
  struct Node {
    AudioNodeStorage::NodeID id{};
    DestinationNode* underlying_destination_node{};
    bool recording_enabled{false};
    AudioParameterWriterID parameter_writer_id{};
    Optional<AudioParameterDescriptor> gain_parameter_descriptor{};
  };

  using ReadNodes = ArrayView<const Node>;

public:
  AudioNodeStorage::NodeID create_node(AudioNodeStorage& node_storage,
                                       AudioGraphRenderer& renderer,
                                       UIAudioParameterManager& ui_parameter_manager,
                                       AudioParameterSystem* parameter_system,
                                       bool acquire_gain_param = true);

  DestinationNode* delete_node(AudioNodeStorage::NodeID id,
                               AudioParameterSystem* parameter_system,
                               UIAudioParameterManager& ui_parameter_manager);

  void set_gain(AudioNodeStorage::NodeID id,
                AudioParameterSystem* parameter_system,
                float to_value);

  void toggle_record_enabled(AudioNodeStorage::NodeID id);

  ReadNodes read_nodes() const;
  int64_t num_nodes() const {
    return nodes.size();
  }

  bool arm_record(AudioRecorder* recorder, const AudioRecordStreamHandle& stream_handle);
  DynamicArray<BufferDataType, 2> record_channel_types() const;

private:
  std::vector<Node> nodes;
};

}