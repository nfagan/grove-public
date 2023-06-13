#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Temporary.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace grove {

class AudioProcessorNode;
class AudioRenderable;
class AudioEffect;

class AudioNodeStorage {
public:
  using NodeID = uint32_t;
  using PortID = uint32_t;

  using AudioProcessorNodeCtor = std::function<AudioProcessorNode*(NodeID)>;
  using AudioRenderableCtor = std::function<AudioRenderable*()>;
  using AudioEffectCtor = std::function<AudioEffect*()>;
  using GatherStaticParameterDescriptors = void(NodeID, TemporaryViewStack<AudioParameterDescriptor>&);

  static constexpr NodeID null_node_id() {
    return 0;
  }

  static constexpr PortID null_port_id() {
    return 0;
  }

  template <typename T>
  using NodeMap = std::unordered_map<NodeID, std::unique_ptr<T>>;

  enum class NodeType : uint8_t {
    AudioProcessorNode,
    AudioRenderable,
    AudioEffect,
  };

  enum class DataType : uint8_t {
    Null = 0,
    Float,
    Sample2,
    MIDIMessage,
    MIDINote,
    Audio,
    MIDIPlusAudio
  };

  enum class PortDirection : uint8_t {
    Input,
    Output
  };

  struct PortDescriptor {
  public:
    bool is_input() const {
      return direction == PortDirection::Input;
    }
    bool is_output() const {
      return direction == PortDirection::Output;
    }
    bool is_optional() const {
      return flags.is_optional();
    }

  public:
    DataType data_type{};
    PortDirection direction{};
    uint8_t index{};
    AudioPort::Flags flags{};
  };

  struct PortInfo {
    bool connected() const {
      return connected_to != null_port_id();
    }

    PortID id{};
    NodeID node_id{};
    PortDescriptor descriptor{};
    PortID connected_to{null_port_id()};
  };

  struct NodeInfo {
    NodeID id{};
    NodeType type{};
    GatherStaticParameterDescriptors* gather_static_parameter_descriptors{};
    bool instance_created{};
  };

  using PortIDs = DynamicArray<uint32_t, 8>;
  using PortDescriptors = DynamicArray<PortDescriptor, 8>;
  using PortInfoForNode = DynamicArray<PortInfo, 8>;

public:
  //  Create an AudioProcessorNode
  NodeID create_node(const AudioProcessorNodeCtor& ctor, const PortDescriptors& port_descriptors,
                     GatherStaticParameterDescriptors* gather_static_descs = nullptr);
  //  Create an AudioRenderable
  NodeID create_node(const AudioRenderableCtor& ctor, const PortDescriptors& port_descriptors);
  //  Create an AudioEffect
  NodeID create_node(const AudioEffectCtor& ctor, const PortDescriptors& port_descriptors);

  void delete_node(NodeID node);

  void construct_instance(NodeID node_id);
  void require_instance(const NodeInfo& info);
  void delete_instance(NodeID node_id);
  bool is_instance_created(NodeID node_id) const;
  bool node_exists(NodeID node_id) const;

  Optional<AudioParameterDescriptor> find_parameter_descriptor(AudioParameterIDs ids) const;
  bool parameter_exists(AudioParameterIDs ids) const;

  AudioProcessorNode* get_audio_processor_node_instance(NodeID node_id) const;
  AudioRenderable* get_audio_renderable_instance(NodeID node_id) const;
  AudioEffect* get_audio_effect_instance(NodeID node_id) const;

  Optional<PortIDs> port_ids_for_node(NodeID node_id) const;
  Optional<PortInfoForNode> get_port_info_for_node(NodeID node_id) const;
  Optional<PortInfo> get_port_info(PortID port_id) const;
  Optional<NodeInfo> get_node_info(NodeID node_id) const;

  ArrayView<AudioParameterDescriptor>
  audio_parameter_descriptors(NodeID for_node, TemporaryViewStack<AudioParameterDescriptor>& mem) const;

  void mark_connected(const PortInfo& port_info, const PortInfo& to);
  void unmark_connected(const PortInfo& port_info);
  bool all_non_optional_ports_connected(NodeID node_id) const;

  int num_audio_processor_nodes() const {
    return int(audio_processor_nodes.size());
  }
  int num_audio_processor_node_ctors() const {
    return int(audio_processor_node_ctors.size());
  }

  static AudioNodeStorage::DataType port_data_type_from_buffer_type(BufferDataType type);

private:
  void erase_audio_processor_node_instance(NodeID node_id);
  void erase_audio_renderable_instance(NodeID node_id);
  void erase_audio_effect_instance(NodeID node_id);

  NodeID create_node_and_port_info(AudioNodeStorage::NodeType node_type,
                                   const PortDescriptors& descriptors,
                                   GatherStaticParameterDescriptors* gather_param_descs);

private:
  template <typename T>
  using CtorMap = std::unordered_map<NodeID, T>;

  NodeID next_node_id{1};
  PortID next_port_id{1};
  std::unordered_map<PortID, PortIDs> port_ids_by_node;

  std::vector<NodeInfo> node_info;
  std::unordered_map<PortID, PortInfo> port_info;

  CtorMap<AudioProcessorNodeCtor> audio_processor_node_ctors;
  CtorMap<AudioRenderableCtor> audio_renderable_ctors;
  CtorMap<AudioEffectCtor> audio_effect_ctors;

  NodeMap<AudioProcessorNode> audio_processor_nodes;
  NodeMap<AudioRenderable> audio_renderables;
  NodeMap<AudioEffect> audio_effects;
};

AudioNodeStorage::PortDescriptors
make_port_descriptors_from_audio_node_ports(AudioProcessorNode* node);

AudioNodeStorage::PortDescriptors
make_port_descriptors_from_audio_node_ctor(const AudioNodeStorage::AudioProcessorNodeCtor& ctor);

AudioNodeStorage::PortDescriptors make_midi_track_port_descriptors();

inline AudioNodeStorage::DataType
AudioNodeStorage::port_data_type_from_buffer_type(BufferDataType type) {
  switch (type) {
    case BufferDataType::Float:
      return AudioNodeStorage::DataType::Float;
    case BufferDataType::Sample2:
      return AudioNodeStorage::DataType::Sample2;
    case BufferDataType::MIDIMessage:
      return AudioNodeStorage::DataType::MIDIMessage;
    default:
      assert(false);
      return AudioNodeStorage::DataType::Float;
  }
}

}