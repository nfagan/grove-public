#include "AudioNodeStorage.hpp"
#include "grove/audio/audio_node.hpp"
#include "grove/audio/AudioRenderable.hpp"
#include "grove/audio/AudioEffect.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using TmpViewParamDesc = TemporaryViewStack<AudioParameterDescriptor>;
using ViewParamDesc = ArrayView<AudioParameterDescriptor>;

template <typename T>
auto find_node(T&& nodes, AudioNodeStorage::NodeID id) {
  return std::lower_bound(nodes.begin(), nodes.end(), id, [](auto& a, auto& b) {
    return a.id < b;
  });
}

template <typename T>
T* maybe_find_instance(const AudioNodeStorage::NodeMap<T>& map, AudioNodeStorage::NodeID node) {
  auto it = map.find(node);
  if (it == map.end()) {
    return nullptr;
  } else {
    return it->second.get();
  }
}

ViewParamDesc to_view(const AudioParameterDescriptors& descs, TmpViewParamDesc& mem) {
  auto* dst = mem.push(int(descs.size()));
  int i{};
  for (auto& desc : descs) {
    dst[i++] = desc;
  }
  return {dst, dst + i};
}

} //  anon

void AudioNodeStorage::delete_node(NodeID node) {
  auto maybe_info = find_node(node_info, node);
  assert(maybe_info != node_info.end());
  assert(maybe_info->id == node);

  if (maybe_info->instance_created) {
    delete_instance(node);
  }

  auto port_id_it = port_ids_by_node.find(node);
  assert(port_id_it != port_ids_by_node.end());

  for (const auto& id : port_id_it->second) {
    port_info.erase(id);
  }

  switch (maybe_info->type) {
    case NodeType::AudioProcessorNode:
      audio_processor_node_ctors.erase(node);
      break;
    case NodeType::AudioRenderable:
      audio_renderable_ctors.erase(node);
      break;
    case NodeType::AudioEffect:
      audio_effect_ctors.erase(node);
      break;
    default:
      assert(false);
  }

  port_ids_by_node.erase(port_id_it);
  node_info.erase(maybe_info);
}

uint32_t AudioNodeStorage::create_node_and_port_info(AudioNodeStorage::NodeType node_type,
                                                     const PortDescriptors& port_descriptors,
                                                     GatherStaticParameterDescriptors* gather_param_descs) {
  auto node_id = next_node_id++;
  NodeInfo new_node_info{node_id, node_type, gather_param_descs};

  port_ids_by_node[node_id] = {};
  auto& port_ids = port_ids_by_node.at(node_id);

  for (const auto& port_descriptor : port_descriptors) {
    auto port_id = next_port_id++;
    PortInfo new_port_info{port_id, node_id, port_descriptor};

    port_info[port_id] = new_port_info;
    port_ids.push_back(port_id);
  }

  auto insert_at = find_node(node_info, node_id);
  assert(insert_at == node_info.end());
  node_info.insert(insert_at, new_node_info);

  return node_id;
}

uint32_t AudioNodeStorage::create_node(const AudioRenderableCtor& ctor,
                                       const PortDescriptors& port_descriptors) {
  auto node_id = create_node_and_port_info(NodeType::AudioRenderable, port_descriptors, nullptr);
  audio_renderable_ctors[node_id] = ctor;
  return node_id;
}

uint32_t AudioNodeStorage::create_node(const AudioProcessorNodeCtor& ctor,
                                       const PortDescriptors& port_descriptors,
                                       GatherStaticParameterDescriptors* gather_param_descs) {
  auto node_id = create_node_and_port_info(
    NodeType::AudioProcessorNode, port_descriptors, gather_param_descs);
  audio_processor_node_ctors[node_id] = ctor;
  return node_id;
}

uint32_t AudioNodeStorage::create_node(const AudioEffectCtor& ctor,
                                       const PortDescriptors& port_descriptors) {
  auto node_id = create_node_and_port_info(NodeType::AudioEffect, port_descriptors, nullptr);
  audio_effect_ctors[node_id] = ctor;
  return node_id;
}

void AudioNodeStorage::require_instance(const NodeInfo& info) {
  if (!info.instance_created) {
    construct_instance(info.id);
  }
}

void AudioNodeStorage::construct_instance(NodeID node_id) {
  auto maybe_info = find_node(node_info, node_id);
  assert(maybe_info != node_info.end() && !maybe_info->instance_created);
  assert(maybe_info->id == node_id);

  switch (maybe_info->type) {
    case NodeType::AudioProcessorNode: {
      auto& node_ctor = audio_processor_node_ctors.at(node_id);
      audio_processor_nodes[node_id] = std::unique_ptr<AudioProcessorNode>(node_ctor(node_id));
      break;
    }
    case NodeType::AudioRenderable: {
      auto& node_ctor = audio_renderable_ctors.at(node_id);
      audio_renderables[node_id] = std::unique_ptr<AudioRenderable>(node_ctor());
      break;
    }
    case NodeType::AudioEffect: {
      auto& node_ctor = audio_effect_ctors.at(node_id);
      audio_effects[node_id] = std::unique_ptr<AudioEffect>(node_ctor());
      break;
    }
    default:
      assert(false);
  }

  maybe_info->instance_created = true;
}

bool AudioNodeStorage::is_instance_created(NodeID node_id) const {
  if (auto it = find_node(node_info, node_id); it != node_info.end() && it->id == node_id) {
    return it->instance_created;
  } else {
    assert(false);
    return false;
  }
}

void AudioNodeStorage::delete_instance(NodeID node_id) {
  auto maybe_info = find_node(node_info, node_id);
  assert(maybe_info != node_info.end() && maybe_info->instance_created);
  assert(maybe_info->id == node_id);

  if (maybe_info->type == NodeType::AudioProcessorNode) {
    erase_audio_processor_node_instance(node_id);

  } else if (maybe_info->type == NodeType::AudioRenderable) {
    erase_audio_renderable_instance(node_id);

  } else if (maybe_info->type == NodeType::AudioEffect) {
    erase_audio_effect_instance(node_id);

  } else {
    assert(false);
  }

  maybe_info->instance_created = false;
}

bool AudioNodeStorage::node_exists(NodeID node_id) const {
  auto it = find_node(node_info, node_id);
  return it != node_info.end() && it->id == node_id;
}

Optional<AudioParameterDescriptor>
AudioNodeStorage::find_parameter_descriptor(AudioParameterIDs ids) const {
  if (node_exists(ids.parent)) {
    Temporary<AudioParameterDescriptor, 512> tmp;
    auto tmp_stack = tmp.view_stack();
    auto params = audio_parameter_descriptors(ids.parent, tmp_stack);

    for (auto& param : params) {
      if (param.ids.self == ids.self) {
        return Optional<AudioParameterDescriptor>(param);
      }
    }
  }

  return NullOpt{};
}

bool AudioNodeStorage::parameter_exists(AudioParameterIDs ids) const {
  return find_parameter_descriptor(ids);
}

void AudioNodeStorage::erase_audio_renderable_instance(NodeID node_id) {
  assert(audio_renderables.count(node_id) > 0);
  audio_renderables.erase(node_id);
}

void AudioNodeStorage::erase_audio_processor_node_instance(NodeID node_id) {
  assert(audio_processor_nodes.count(node_id) > 0);
  audio_processor_nodes.erase(node_id);
}

void AudioNodeStorage::erase_audio_effect_instance(NodeID node_id) {
  assert(audio_effects.count(node_id) > 0);
  audio_effects.erase(node_id);
}

AudioProcessorNode* AudioNodeStorage::get_audio_processor_node_instance(NodeID node_id) const {
  return maybe_find_instance(audio_processor_nodes, node_id);
}

AudioRenderable* AudioNodeStorage::get_audio_renderable_instance(NodeID node_id) const {
  return maybe_find_instance(audio_renderables, node_id);
}

AudioEffect* AudioNodeStorage::get_audio_effect_instance(NodeID node_id) const {
  return maybe_find_instance(audio_effects, node_id);
}

Optional<AudioNodeStorage::PortInfoForNode>
AudioNodeStorage::get_port_info_for_node(NodeID node_id) const {
  AudioNodeStorage::PortInfoForNode info_for_node;
  auto it = port_ids_by_node.find(node_id);

  if (it != port_ids_by_node.end()) {
    for (const auto& id : it->second) {
      assert(port_info.count(id) > 0);
      auto& info = port_info.at(id);
      info_for_node.push_back(info);
    }

    return Optional<AudioNodeStorage::PortInfoForNode>(std::move(info_for_node));

  } else {
    return NullOpt{};
  }
}

ArrayView<AudioParameterDescriptor>
AudioNodeStorage::audio_parameter_descriptors(NodeID for_node, TmpViewParamDesc& mem) const {
  auto it = find_node(node_info, for_node);
  if (it == node_info.end() || it->id != for_node) {
    //  Node should exist.
    assert(false);
    return {};
  }

  switch (it->type) {
    case NodeType::AudioProcessorNode: {
      if (it->gather_static_parameter_descriptors) {
        it->gather_static_parameter_descriptors(it->id, mem);
        return ArrayView<AudioParameterDescriptor>{mem.begin(), mem.end()};

      } else if (it->instance_created) {
        get_audio_processor_node_instance(it->id)->parameter_descriptors(mem);
        return ArrayView<AudioParameterDescriptor>{mem.begin(), mem.end()};

      } else {
        return {};
      }
    }
    case NodeType::AudioRenderable:
      return {};
    case NodeType::AudioEffect: {
      if (it->instance_created) {
        return to_view(get_audio_effect_instance(it->id)->parameter_descriptors(), mem);
      } else {
        return {};
      }
    }
    default:
      assert(false);
      return {};
  }
}

Optional<AudioNodeStorage::PortInfo> AudioNodeStorage::get_port_info(PortID port_id) const {
  auto it = port_info.find(port_id);
  return it == port_info.end() ? NullOpt{} : Optional<PortInfo>(it->second);
}

Optional<AudioNodeStorage::NodeInfo> AudioNodeStorage::get_node_info(NodeID node_id) const {
  auto it = find_node(node_info, node_id);
  return (it == node_info.end() || it->id != node_id) ? NullOpt{} : Optional<NodeInfo>(*it);
}

Optional<AudioNodeStorage::PortIDs> AudioNodeStorage::port_ids_for_node(NodeID node_id) const {
  auto it = port_ids_by_node.find(node_id);
  return it == port_ids_by_node.end() ? NullOpt{} : Optional<PortIDs>(it->second);
}

void AudioNodeStorage::mark_connected(const PortInfo& info, const PortInfo& to) {
  auto it = port_info.find(info.id);
  assert(it != port_info.end());
  it->second.connected_to = to.id;
}

void AudioNodeStorage::unmark_connected(const PortInfo& info) {
  auto it = port_info.find(info.id);
  assert(it != port_info.end());
  it->second.connected_to = null_port_id();
}

bool AudioNodeStorage::all_non_optional_ports_connected(NodeID node_id) const {
  if (auto id_it = port_ids_by_node.find(node_id); id_it != port_ids_by_node.end()) {
    auto& ids = id_it->second;
    for (auto& id : ids) {
      if (auto info_it = port_info.find(id); info_it != port_info.end()) {
        auto& info = info_it->second;
        if (!info.connected() && !info.descriptor.is_optional()) {
          return false;
        }
      } else {
        assert(false);
        return false;
      }
    }
  } else {
    assert(false);
    return false;
  }

  return true;
}

AudioNodeStorage::PortDescriptors
make_port_descriptors_from_audio_node_ctor(const AudioNodeStorage::AudioProcessorNodeCtor& ctor) {
  std::unique_ptr<AudioProcessorNode> ref_node(ctor(0));
  return make_port_descriptors_from_audio_node_ports(ref_node.get());
}

AudioNodeStorage::PortDescriptors
make_port_descriptors_from_audio_node_ports(AudioProcessorNode* node) {
  AudioNodeStorage::PortDescriptors port_descriptors;

  auto outputs = node->outputs();
  auto inputs = node->inputs();

  uint8_t out_ind = 0;
  for (const auto& out : outputs) {
    port_descriptors.push_back({
      AudioNodeStorage::port_data_type_from_buffer_type(out.type),
      AudioNodeStorage::PortDirection::Output,
      out_ind++,
      out.flags
    });
  }

  uint8_t in_ind = 0;
  for (const auto& in : inputs) {
    port_descriptors.push_back({
      AudioNodeStorage::port_data_type_from_buffer_type(in.type),
      AudioNodeStorage::PortDirection::Input,
      in_ind++,
      in.flags
    });
  }

  return port_descriptors;
}

AudioNodeStorage::PortDescriptors make_midi_track_port_descriptors() {
  using DataType = AudioNodeStorage::DataType;
  using PortDirection = AudioNodeStorage::PortDirection;

  AudioNodeStorage::PortDescriptors port_descriptors;

  AudioNodeStorage::PortDescriptor audio_out_port{
    DataType::Audio, PortDirection::Output, 0
  };
  AudioNodeStorage::PortDescriptor midi_out_port{
    DataType::MIDINote, PortDirection::Output, 1
  };
  AudioNodeStorage::PortDescriptor midi_plus_audio_port{
    DataType::MIDIPlusAudio, PortDirection::Output, 2
  };

  port_descriptors.push_back(audio_out_port);
  port_descriptors.push_back(midi_out_port);
  port_descriptors.push_back(midi_plus_audio_port);

  return port_descriptors;
}

GROVE_NAMESPACE_END
