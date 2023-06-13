#include "audio_nodes.hpp"
#include "../audio_processors/TriggeredOsc.hpp"
#include "../audio_processors/RhythmicDelay1.hpp"
#include "../audio_processors/AltReverbNode.hpp"
#include "../audio_observation/RhythmicDelay1.hpp"
#include "../audio_observation/RandomizedEnvelope.hpp"
#include "../audio_observation/AltReverbNode.hpp"
#include "../audio_observation/TriggeredOsc.hpp"
#include "../audio_observation/AudioObservation.hpp"
#include "../audio_core/NodeSignalValueSystem.hpp"
#include "grove/audio/audio_processor_nodes/RandomizedEnvelopeNode.hpp"
#include "grove/common/common.hpp"
#include "grove/audio/AudioParameterSystem.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using Context = ProceduralTreeAudioNodes::Context;
using NodeToDelete = ProceduralTreeAudioNodes::NodeToDelete;
using DelayNodeInfo = ProceduralTreeAudioNodes::DelayNodeInfo;
using EnvelopeNodeInfo = ProceduralTreeAudioNodes::EnvelopeNodeInfo;
using ReverbNodeInfo = ProceduralTreeAudioNodes::ReverbNodeInfo;
using TriggeredOscNodeInfo = ProceduralTreeAudioNodes::TriggeredOscNodeInfo;
using DelayNodes = ProceduralTreeAudioNodes::TreeIDMap<DelayNodeInfo>;
using ReverbNodes = ProceduralTreeAudioNodes::TreeIDMap<ReverbNodeInfo>;
using EnvelopeNodes = ProceduralTreeAudioNodes::TreeIDMap<EnvelopeNodeInfo>;
using TriggeredOscNodes = ProceduralTreeAudioNodes::TreeIDMap<TriggeredOscNodeInfo>;
using MonitorableNode = observe::AudioParameterMonitor::MonitorableNode;
using MakeMonitorableNode = std::function<MonitorableNode(ProceduralTreeInstrument::Instance&&)>;
using PendingPortPlacement = ProceduralTreeAudioNodes::PendingPortPlacement;
using ReleaseParameterWrite = ProceduralTreeAudioNodes::ReleaseParameterWrite;
using ReleaseParameterWrites = DynamicArray<ReleaseParameterWrite, 8>;
using RemoveNodeResult = ProceduralTreeAudioNodes::RemoveNodeResult;

bool can_gather_parameter_ids(const AudioNodeStorage& node_storage, AudioNodeStorage::NodeID id) {
  if (auto maybe_node_info = node_storage.get_node_info(id)) {
    if (maybe_node_info.value().instance_created) {
      if (node_storage.all_non_optional_ports_connected(id)) {
        return true;
      }
    }
  }
  return false;
}

template <typename T>
Optional<AudioParameterDescriptor> find_descriptor(T&& descriptors, const char* name) {
  auto filtered = filter_audio_parameter_descriptors(descriptors, [name](auto&& descr) {
    return descr.matches_name(name);
  });
  return filtered.size() == 1 ? Optional<AudioParameterDescriptor>(*filtered[0]) : NullOpt{};
}

void gather_delay_node_parameter_ids(DelayNodes& delay_nodes, const Context& context) {
  const AudioParameterWriterID param_writer_id = context.parameter_writer;
  for (auto& [_, node_info] : delay_nodes) {
    const auto node_id = node_info.node_id;
    if (!can_gather_parameter_ids(context.node_storage, node_id)) {
      continue;
    }
    if (!node_info.chorus_mix_param_ids) {
      Temporary<AudioParameterDescriptor, 16> tmp_desc;
      auto tmp_view_desc = tmp_desc.view_stack();
      auto params = context.node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);
      auto chorus_mix = filter_audio_parameter_descriptors(params, [](auto&& descr) {
        return descr.matches_name("chorus_mix");
      });
      if (chorus_mix.size() == 1) {
        auto ids = chorus_mix[0]->ids;
        auto& write_access = *param_system::ui_get_write_access(&context.parameter_system);
        if (write_access.request(param_writer_id, ids)) {
          node_info.chorus_mix_param_ids = ids;
        }
      }
    }
    if (!node_info.noise_mix_param_ids) {
      Temporary<AudioParameterDescriptor, 16> tmp_desc;
      auto tmp_view_desc = tmp_desc.view_stack();
      auto params = context.node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);
      auto noise_mix = filter_audio_parameter_descriptors(params, [](auto&& descr) {
        return descr.matches_name("noise_mix");
      });
      if (noise_mix.size() == 1) {
        auto ids = noise_mix[0]->ids;
        auto& write_access = *param_system::ui_get_write_access(&context.parameter_system);
        if (write_access.request(param_writer_id, ids)) {
          node_info.noise_mix_param_ids = ids;
        }
      }
    }
  }
}

void gather_envelope_node_parameter_ids(EnvelopeNodes& envelope_nodes, const Context& context) {
  const AudioParameterWriterID param_writer_id = context.parameter_writer;
  for (auto& [_, node_info] : envelope_nodes) {
    if (!node_info.amp_mod_descriptor) {
      const auto node_id = node_info.node_id;
      if (!can_gather_parameter_ids(context.node_storage, node_id)) {
        continue;
      }

      Temporary<AudioParameterDescriptor, 16> tmp_desc;
      auto tmp_view_desc = tmp_desc.view_stack();
      auto params = context.node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);
      auto amp_mod = filter_audio_parameter_descriptors(params, [](auto&& descr) {
        return descr.matches_name("amplitude_modulation_amount");
      });
      if (amp_mod.size() == 1) {
        auto& write_access = *param_system::ui_get_write_access(&context.parameter_system);
        auto& ids = amp_mod[0]->ids;
        if (write_access.request(param_writer_id, ids)) {
          node_info.amp_mod_descriptor = *amp_mod[0];
        }
      }
    }
  }
}

Optional<AudioParameterIDs> acquire_write_access(
  const Context& context, uint32_t node_id, const char* param) {
  //
  Temporary<AudioParameterDescriptor, 16> tmp_desc;
  auto tmp_view_desc = tmp_desc.view_stack();
  auto params = context.node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);

  if (auto desc = find_descriptor(params, param)) {
    const AudioParameterWriterID param_writer_id = context.parameter_writer;
    auto ids = desc.value().ids;
    auto& write_access = *param_system::ui_get_write_access(&context.parameter_system);
    if (write_access.request(param_writer_id, ids)) {
      return Optional<AudioParameterIDs>(ids);
    }
  }

  return NullOpt{};
}

void gather_reverb_node_parameter_ids(ReverbNodes& reverb_nodes, const Context& context) {
  for (auto& [_, node_info] : reverb_nodes) {
    const auto node_id = node_info.node_id;
    if (!can_gather_parameter_ids(context.node_storage, node_id)) {
      continue;
    }
    if (!node_info.mix_param_ids) {
      if (auto ids = acquire_write_access(context, node_id, "mix")) {
        node_info.mix_param_ids = ids.value();
      }
    }
    if (!node_info.fb_param_ids) {
      if (auto ids = acquire_write_access(context, node_id, "feedback")) {
        node_info.fb_param_ids = ids.value();
      }
    }
    if (!node_info.fixed_osc_mix_param_ids) {
      if (auto ids = acquire_write_access(context, node_id, "fixed_osc_mix")) {
        node_info.fixed_osc_mix_param_ids = ids.value();
      }
    }
  }
}

void gather_triggered_osc_node_parameter_ids(TriggeredOscNodes& nodes, const Context& context) {
  const AudioParameterWriterID param_writer_id = context.parameter_writer;

  for (auto& [_, node_info] : nodes) {
    const auto node_id = node_info.node_id;
    if (!can_gather_parameter_ids(context.node_storage, node_id)) {
      continue;
    }
    if (node_info.signal_param_ids && node_info.monitor_note_number_param_ids &&
        node_info.semitone_offset_desc) {
      continue;
    }

    Temporary<AudioParameterDescriptor, 32> tmp_desc;
    auto tmp_view_desc = tmp_desc.view_stack();
    auto params = context.node_storage.audio_parameter_descriptors(node_id, tmp_view_desc);

    if (!node_info.signal_param_ids) {
      if (auto desc = find_descriptor(params, "signal_representation")) {
        node_info.signal_param_ids = desc.value().ids;
      }
    }
    if (!node_info.monitor_note_number_param_ids) {
      if (auto desc = find_descriptor(params, "monitor_note_number")) {
        node_info.monitor_note_number_param_ids = desc.value().ids;
      }
    }
    if (!node_info.semitone_offset_desc) {
      if (auto desc = find_descriptor(params, "semitone_offset")) {
        auto& write_access = *param_system::ui_get_write_access(&context.parameter_system);
        if (write_access.request(param_writer_id, desc.value().ids)) {
          node_info.semitone_offset_desc = desc.value();
        }
      }
    }
  }
}

template <typename Nodes>
auto* find_node_info(Nodes& nodes, tree::TreeID id) {
  auto it = nodes.find(id);
  if (it != nodes.end()) {
    return &it->second;
  } else {
    decltype(&it->second) null{};
    return null;
  }
}

PendingPortPlacement make_instrument(const Context& context,
                                     AudioNodeStorage::AudioProcessorNodeCtor&& node_ctor,
                                     MakeMonitorableNode&& make_monitorable_node,
                                     const Vec3f& position, float port_y_offset,
                                     AudioNodeStorage::NodeID* out_node_id) {
  auto port_descriptors = make_port_descriptors_from_audio_node_ctor(node_ctor);
  auto node = context.node_storage.create_node(node_ctor, port_descriptors);
  auto instr = context.tree_instrument.create_instance(node);
  auto port_info = context.node_storage.get_port_info_for_node(node).unwrap();

  auto monitorable_node = make_monitorable_node(std::move(instr));
  context.audio_observation.parameter_monitor.add_node(node, std::move(monitorable_node));

  *out_node_id = node;

  PendingPortPlacement result{};
  result.node_id = node;
  result.position = position;
  result.port_info = std::move(port_info);
  result.y_offset = port_y_offset;

  return result;
}

PendingPortPlacement make_delay(const Context& context, const Vec3f& position,
                                float port_y_offset, AudioNodeStorage::NodeID* out_node_id) {
  auto* param_system = &context.parameter_system;
  auto node_ctor = [param_system](AudioNodeStorage::NodeID node_id) {
    return new RhythmicDelay1(node_id, param_system);
  };
  auto make_monitorable_node = [](ProceduralTreeInstrument::Instance&& inst) {
    return observe::RhythmicDelay1::make_node(std::move(inst.callback));
  };
  return make_instrument(
    context, std::move(node_ctor), std::move(make_monitorable_node),
    position, port_y_offset, out_node_id);
}

PendingPortPlacement make_envelope(const Context& context, const Vec3f& position,
                                   float port_y_offset, AudioNodeStorage::NodeID* out_node_id) {
  auto* param_system = &context.parameter_system;
  auto node_ctor = [param_system](AudioNodeStorage::NodeID node_id) {
    const int num_outputs = 1;
    const bool emit_events = true;
    return new RandomizedEnvelopeNode(node_id, param_system, num_outputs, emit_events);
  };
  auto make_monitorable_node = [](ProceduralTreeInstrument::Instance&& inst) {
    return observe::RandomizedEnvelope::make_node(std::move(inst.callback));
  };
  return make_instrument(
    context, std::move(node_ctor), std::move(make_monitorable_node),
    position, port_y_offset, out_node_id);
}

PendingPortPlacement make_reverb(const Context& context, const Vec3f& position,
                                 float port_y_offset, AudioNodeStorage::NodeID* out_node_id) {
  auto* param_system = &context.parameter_system;
  auto node_ctor = [param_system](AudioNodeStorage::NodeID node_id) {
    return new AltReverbNode(node_id, param_system);
  };
  auto make_monitorable_node = [](ProceduralTreeInstrument::Instance&& inst) {
    return observe::AltReverbNode::make_node(std::move(inst.callback));
  };

  return make_instrument(
    context, std::move(node_ctor), std::move(make_monitorable_node),
    position, port_y_offset, out_node_id);
}

PendingPortPlacement make_triggered_osc(const Context& context, const Vec3f& position,
                                        float port_y_offset, AudioNodeStorage::NodeID* out_node_id) {
  auto* param_system = &context.parameter_system;
  auto* scale = &context.audio_scale;
  auto node_ctor = [param_system, scale](AudioNodeStorage::NodeID node_id) {
    return new TriggeredOsc(node_id, scale, param_system);
  };
  auto make_monitorable_node = [](ProceduralTreeInstrument::Instance&& inst) {
    auto cb0 = inst.callback;
    auto cb1 = inst.callback;
    return observe::TriggeredOsc::make_node(std::move(cb0), std::move(cb1));
  };

  return make_instrument(
    context, std::move(node_ctor), std::move(make_monitorable_node),
    position, port_y_offset, out_node_id);
}

ReleaseParameterWrite make_released_parameter_write(AudioParameterWriterID writer_id,
                                                    const AudioParameterIDs& param_ids) {
  return {writer_id, param_ids};
}

NodeToDelete make_node_to_delete(AudioNodeStorage::NodeID id, bool remove_placed_node) {
  NodeToDelete to_del;
  to_del.id = id;
  to_del.remove_placed_node = remove_placed_node;
  return to_del;
}

ReleaseParameterWrites make_released_parameter_writes(const DelayNodeInfo& node_info,
                                                      AudioParameterWriterID writer_id) {
  ReleaseParameterWrites result;
  if (node_info.chorus_mix_param_ids) {
    result.push_back(make_released_parameter_write(
      writer_id, node_info.chorus_mix_param_ids.value()));
  }
  if (node_info.noise_mix_param_ids) {
    result.push_back(make_released_parameter_write(
      writer_id, node_info.noise_mix_param_ids.value()));
  }
  return result;
}

ReleaseParameterWrites make_released_parameter_writes(const EnvelopeNodeInfo& node_info,
                                                      AudioParameterWriterID writer_id) {
  ReleaseParameterWrites result;
  if (node_info.amp_mod_descriptor) {
    result.push_back(
      make_released_parameter_write(writer_id, node_info.amp_mod_descriptor.value().ids));
  }
  return result;
}

ReleaseParameterWrites make_released_parameter_writes(const ReverbNodeInfo& node_info,
                                                      AudioParameterWriterID writer_id) {
  ReleaseParameterWrites result;
  if (node_info.mix_param_ids) {
    result.push_back(
      make_released_parameter_write(writer_id, node_info.mix_param_ids.value()));
  }
  if (node_info.fb_param_ids) {
    result.push_back(
      make_released_parameter_write(writer_id, node_info.fb_param_ids.value()));
  }
  if (node_info.fixed_osc_mix_param_ids) {
    result.push_back(
      make_released_parameter_write(writer_id, node_info.fixed_osc_mix_param_ids.value()));
  }
  return result;
}

ReleaseParameterWrites make_released_parameter_writes(const TriggeredOscNodeInfo& node_info,
                                                      AudioParameterWriterID writer_id) {
  ReleaseParameterWrites result;
  if (node_info.semitone_offset_desc) {
    result.push_back(
      make_released_parameter_write(writer_id, node_info.semitone_offset_desc.value().ids));
  }
  return result;
}

template <typename T>
RemoveNodeResult destroy_node(ProceduralTreeAudioNodes& audio_nodes,
                              T& nodes, const Context& context, tree::TreeID id,
                              bool remove_placed_node) {
  audio_nodes.released_parameter_writes.clear();

  auto it = nodes.find(id);
  if (it == nodes.end()) {
    assert(false);
    return {};
  }

  auto& info = it->second;
  for (auto& to_release : make_released_parameter_writes(info, context.parameter_writer)) {
    audio_nodes.released_parameter_writes.push_back(to_release);
  }

  const AudioNodeStorage::NodeID erased_id = info.node_id;
  auto to_del = make_node_to_delete(erased_id, remove_placed_node);

  RemoveNodeResult result{};
  result.pending_deletion = to_del;
  result.release_parameter_writes = make_view(audio_nodes.released_parameter_writes);
  nodes.erase(it);
  audio_nodes.audio_node_id_to_tree_id.erase(erased_id);
  return result;
}

template <typename T>
void reset_signal_values(T& map) {
  for (auto& [_, node_info] : map) {
    node_info.signal_value = NullOpt{};
  }
}

template <typename T>
void push_signal_values(audio::NodeSignalValueSystem* sys, const T& map) {
  for (auto& [node_id, node_info] : map) {
    if (node_info.signal_value) {
      audio::set_node_signal_value01(sys, node_info.node_id, node_info.signal_value.value());
    }
  }
}

bool has_delay_node(const ProceduralTreeAudioNodes& nodes, tree::TreeID id) {
  return nodes.delay_nodes.count(id) > 0;
}
bool has_envelope_node(const ProceduralTreeAudioNodes& nodes, tree::TreeID id) {
  return nodes.envelope_nodes.count(id) > 0;
}
bool has_reverb_node(const ProceduralTreeAudioNodes& nodes, tree::TreeID id) {
  return nodes.reverb_nodes.count(id) > 0;
}
bool has_triggered_osc_node(const ProceduralTreeAudioNodes& nodes, tree::TreeID id) {
  return nodes.triggered_osc_nodes.count(id) > 0;
}

} //  anon

void ProceduralTreeAudioNodes::gather_parameter_ids(const Context& context) {
  gather_delay_node_parameter_ids(delay_nodes, context);
  gather_envelope_node_parameter_ids(envelope_nodes, context);
  gather_reverb_node_parameter_ids(reverb_nodes, context);
  gather_triggered_osc_node_parameter_ids(triggered_osc_nodes, context);
}

using ParamChanges = ArrayView<const ProceduralTreeInstrument::ObservableChange>;
void ProceduralTreeAudioNodes::process_monitorable_changes(
  audio::NodeSignalValueSystem* node_signal_value_system, const ParamChanges& changes) {
  //
  reset_signal_values(delay_nodes);
  reset_signal_values(envelope_nodes);
  reset_signal_values(reverb_nodes);
  reset_signal_values(triggered_osc_nodes);

  for (auto& change : changes) {
    auto node_id_it = audio_node_id_to_tree_id.find(change.node_id);
    if (node_id_it == audio_node_id_to_tree_id.end()) {
      //  Tree has been removed but some audio events from the underlying instrument
      //  were / are still being processed.
      continue;
    }

    const tree::TreeID tree_id = node_id_it->second;
    if (auto* delay_info = find_node_info(delay_nodes, tree_id)) {
      delay_info->signal_value = change.value;

    } else if (auto* env_info = find_node_info(envelope_nodes, tree_id)) {
      env_info->signal_value = change.value;

    } else if (auto* rev_info = find_node_info(reverb_nodes, tree_id)) {
      rev_info->signal_value = change.value;

    } else if (auto* osc_info = find_node_info(triggered_osc_nodes, tree_id)) {
      auto id = change.parameter_id;
      if (osc_info->signal_param_ids && osc_info->signal_param_ids.value().self == id) {
        osc_info->signal_value = change.value;
      } else if (osc_info->monitor_note_number_param_ids &&
                 osc_info->monitor_note_number_param_ids.value().self == id) {
        osc_info->note_number_value = change.value;
      }
    }
  }

  push_signal_values(node_signal_value_system, delay_nodes);
  push_signal_values(node_signal_value_system, envelope_nodes);
  push_signal_values(node_signal_value_system, reverb_nodes);
  push_signal_values(node_signal_value_system, triggered_osc_nodes);
}

Optional<float> ProceduralTreeAudioNodes::get_signal_value(tree::TreeID id) const {
  if (auto* delay_info = find_node_info(delay_nodes, id)) {
    return delay_info->signal_value;
  } else if (auto* env_info = find_node_info(envelope_nodes, id)) {
    return env_info->signal_value;
  } else if (auto* rev_info = find_node_info(reverb_nodes, id)) {
    return rev_info->signal_value;
  } else if (auto* osc_info = find_node_info(triggered_osc_nodes, id)) {
    return osc_info->signal_value;
  } else {
    return NullOpt{};
  }
}

Optional<float> ProceduralTreeAudioNodes::get_triggered_osc_note_number_value(tree::TreeID id) const {
  if (auto* osc_info = find_node_info(triggered_osc_nodes, id)) {
    return osc_info->note_number_value;
  } else {
    return NullOpt{};
  }
}

PendingPortPlacement
ProceduralTreeAudioNodes::create_delay_node(const Context& context, tree::TreeID tree_id,
                                            const Vec3f& pos, float port_y_offset) {
  AudioNodeStorage::NodeID id{};
  auto res = make_delay(context, pos, port_y_offset, &id);
  DelayNodeInfo node_info{};
  node_info.node_id = id;
  node_info.position = pos;
  delay_nodes[tree_id] = node_info;
  assert(id != AudioNodeStorage::null_node_id());
  audio_node_id_to_tree_id[id] = tree_id;
  return res;
}

PendingPortPlacement
ProceduralTreeAudioNodes::create_envelope_node(const Context& context, tree::TreeID tree_id,
                                               const Vec3f& pos, float port_y_offset) {
  AudioNodeStorage::NodeID id{};
  auto res = make_envelope(context, pos, port_y_offset, &id);
  EnvelopeNodeInfo node_info{};
  node_info.node_id = id;
  node_info.position = pos;
  envelope_nodes[tree_id] = node_info;
  assert(id != AudioNodeStorage::null_node_id());
  audio_node_id_to_tree_id[id] = tree_id;
  return res;
}

PendingPortPlacement
ProceduralTreeAudioNodes::create_reverb_node(const Context& context, tree::TreeID tree_id,
                                             const Vec3f& pos, float port_y_offset) {
  AudioNodeStorage::NodeID id{};
  auto res = make_reverb(context, pos, port_y_offset, &id);
  ReverbNodeInfo node_info{};
  node_info.node_id = id;
  node_info.position = pos;
  reverb_nodes[tree_id] = node_info;
  assert(id != AudioNodeStorage::null_node_id());
  audio_node_id_to_tree_id[id] = tree_id;
  return res;
}

PendingPortPlacement
ProceduralTreeAudioNodes::create_triggered_osc_node(const Context& context, tree::TreeID tree_id,
                                                    const Vec3f& pos, float port_y_offset) {
  AudioNodeStorage::NodeID id{};
  auto res = make_triggered_osc(context, pos, port_y_offset, &id);
  TriggeredOscNodeInfo node_info{};
  node_info.node_id = id;
  node_info.position = pos;
  triggered_osc_nodes[tree_id] = node_info;
  assert(id != AudioNodeStorage::null_node_id());
  audio_node_id_to_tree_id[id] = tree_id;
  return res;
}

RemoveNodeResult ProceduralTreeAudioNodes::destroy_node(const Context& context, tree::TreeID id,
                                                        bool remove_placed_node) {
  if (has_delay_node(*this, id)) {
    return grove::destroy_node(*this, delay_nodes, context, id, remove_placed_node);
  } else if (has_envelope_node(*this, id)) {
    return grove::destroy_node(*this, envelope_nodes, context, id, remove_placed_node);
  } else if (has_reverb_node(*this, id)) {
    return grove::destroy_node(*this, reverb_nodes, context, id, remove_placed_node);
  } else if (has_triggered_osc_node(*this, id)) {
    return grove::destroy_node(*this, triggered_osc_nodes, context, id, remove_placed_node);
  } else {
    assert(false);
    return {};
  }
}

GROVE_NAMESPACE_END
