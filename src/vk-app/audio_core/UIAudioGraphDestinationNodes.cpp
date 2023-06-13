#include "UIAudioGraphDestinationNodes.hpp"
#include "UIAudioParameterManager.hpp"
#include "vk-app/audio_processors/WrapDestinationNode.hpp"
#include "grove/audio/AudioGraphRenderer.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr int num_destination_node_outputs() {
  return 2;
}

constexpr BufferDataType destination_node_sample_type() {
  return BufferDataType::Float;
}

template <typename T>
auto find_node(T&& nodes, AudioNodeStorage::NodeID id) {
  return std::find_if(nodes.begin(), nodes.end(), [id](auto&& node) {
    return node.id == id;
  });
}

Optional<AudioParameterDescriptor>
find_gain_descriptor(const AudioProcessorNode* instance,
                     TemporaryViewStack<AudioParameterDescriptor>& mem) {
  instance->parameter_descriptors(mem);
  auto param_desc = ArrayView<AudioParameterDescriptor>{mem.begin(), mem.end()};
  auto gain_descriptor = filter_audio_parameter_descriptors(param_desc, [](auto&& descr) {
    return descr.matches_name("gain");
  });
  if (gain_descriptor.size() == 1) {
    return Optional<AudioParameterDescriptor>(*gain_descriptor[0]);
  } else {
    return NullOpt{};
  }
}

} //  anon

AudioNodeStorage::NodeID
UIAudioGraphDestinationNodes::create_node(AudioNodeStorage& node_storage,
                                          AudioGraphRenderer& renderer,
                                          UIAudioParameterManager& ui_parameter_manager,
                                          AudioParameterSystem* parameter_system,
                                          bool acquire_gain_param) {
  const int num_outputs = num_destination_node_outputs();
  auto ref_node = std::make_unique<DestinationNode>(0, parameter_system, num_outputs);
  auto port_descriptors = make_port_descriptors_from_audio_node_ports(ref_node.get());
  auto destination = renderer.create_destination(0, parameter_system, num_outputs);

  auto dest_node_ctor = [destination](AudioNodeStorage::NodeID node_id) {
    //  @TODO: This wrapping is only required because the renderer owns the destination node.
    //  Change the renderer API to support adding and removing raw destination node pointers.
    destination->set_node_id(node_id);
    return new WrapDestinationNode(destination);
  };

  auto dest_node_id = node_storage.create_node(dest_node_ctor, port_descriptors);
  node_storage.construct_instance(dest_node_id);

  //  Obtain write access to the gain parameter.
  const auto* instance = node_storage.get_audio_processor_node_instance(dest_node_id);
  auto& param_write_access = *param_system::ui_get_write_access(parameter_system);

  Optional<AudioParameterDescriptor> maybe_gain_descriptor;
  AudioParameterWriterID param_writer_id{};

  if (acquire_gain_param) {
    Temporary<AudioParameterDescriptor, 32> tmp_desc;
    auto tmp_view_desc = tmp_desc.view_stack();
    maybe_gain_descriptor = find_gain_descriptor(instance, tmp_view_desc);

    if (maybe_gain_descriptor) {
      param_writer_id = AudioParameterWriteAccess::create_writer();
      auto& descriptor = maybe_gain_descriptor.value();

      if (param_write_access.request(param_writer_id, descriptor)) {
        auto param_val = UIAudioParameter::from_descriptor(descriptor);
        ui_parameter_manager.add_active_ui_parameter(descriptor.ids, param_val);

      } else {
        assert(false);
        maybe_gain_descriptor = NullOpt{};
      }
    }
  }

  Node node{};
  node.id = dest_node_id;
  node.underlying_destination_node = destination;
  node.gain_parameter_descriptor = maybe_gain_descriptor;
  node.parameter_writer_id = param_writer_id;
  nodes.push_back(node);

  return dest_node_id;
}

DestinationNode* UIAudioGraphDestinationNodes::delete_node(AudioNodeStorage::NodeID id,
                                                           AudioParameterSystem* parameter_system,
                                                           UIAudioParameterManager& parameter_manager) {
  auto it = find_node(nodes, id);
  if (it != nodes.end()) {
    auto& node = *it;
    param_system::ui_remove_parent(parameter_system, node.id);

    //  Release write access, if acquired.
    auto& param_write_access = *param_system::ui_get_write_access(parameter_system);
    if (node.gain_parameter_descriptor) {
      auto& descriptor = node.gain_parameter_descriptor.value();
      param_write_access.release(node.parameter_writer_id, descriptor);
      parameter_manager.remove_active_ui_parameter(descriptor.ids);
    }

    auto underlying_node = node.underlying_destination_node;
    nodes.erase(it);
    return underlying_node;

  } else {
    return nullptr;
  }
}

void UIAudioGraphDestinationNodes::set_gain(AudioNodeStorage::NodeID id,
                                            AudioParameterSystem* parameter_system,
                                            float to_value) {
  auto it = find_node(nodes, id);
  if (it != nodes.end()) {
    auto& node = *it;
    if (node.gain_parameter_descriptor) {
      to_value = grove::clamp(to_value, 0.0f, 1.0f);
      auto& desc = node.gain_parameter_descriptor.value();
      auto val = make_interpolated_parameter_value_from_descriptor(desc, to_value);
      param_system::ui_set_value(parameter_system, node.parameter_writer_id, desc.ids, val);
    }
  }
}

void UIAudioGraphDestinationNodes::toggle_record_enabled(AudioNodeStorage::NodeID id) {
  auto it = find_node(nodes, id);
  if (it != nodes.end()) {
    it->recording_enabled = !it->recording_enabled;
  } else {
    assert(false);
  }
}

UIAudioGraphDestinationNodes::ReadNodes UIAudioGraphDestinationNodes::read_nodes() const {
  return make_data_array_view<const Node>(nodes);
}

bool UIAudioGraphDestinationNodes::arm_record(AudioRecorder* recorder,
                                              const AudioRecordStreamHandle& stream_handle) {
  for (auto& node : nodes) {
    if (node.recording_enabled) {
      if (!node.underlying_destination_node->set_record_info({recorder, stream_handle})) {
        return false;
      }
    }
  }

  return true;
}

DynamicArray<BufferDataType, 2> UIAudioGraphDestinationNodes::record_channel_types() const {
  DynamicArray<BufferDataType, 2> result;
  for (int i = 0; i < num_destination_node_outputs(); i++) {
    result.push_back(destination_node_sample_type());
  }
  return result;
}

GROVE_NAMESPACE_END
