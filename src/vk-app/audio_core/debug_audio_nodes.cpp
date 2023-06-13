#include "debug_audio_nodes.hpp"
#include "AudioComponent.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_processors/SimpleFM1.hpp"
#include "../audio_processors/OscSwell.hpp"
#include "../audio_processors/MultiComponentSampler.hpp"
#include "../audio_processors/ChimeSampler.hpp"
#include "../audio_processors/AltReverbNode.hpp"
#include "../audio_processors/Skittering1.hpp"
#include "../audio_processors/GaussDistributedPitches1.hpp"
#include "../audio_processors/TransientsSampler1.hpp"
#include "../audio_processors/DebugTuning.hpp"
#include "grove/audio/io.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include <imgui/imgui.h>
#include <vector>
#include <string>

GROVE_NAMESPACE_BEGIN

namespace {

struct DebugNode {
  uint32_t id;
  const char* name;
};

using MakeNode = DebugNode(const debug::DebugAudioNodesContext&);

uint32_t get_ith_pitch_sample_group_id(const AudioComponent& component, uint32_t i) {
  PitchSampleSetGroupHandle handle = pss::ui_get_ith_group(
    component.get_pitch_sampling_system(), i);
  return handle.id;
}

DebugNode create_debug_tuning(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;

  auto node_ctor = [](AudioNodeStorage::NodeID node_id) {
    return new DebugTuning(node_id);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "DebugTuning";
  return node;
}

DebugNode create_transients_sampler1(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* param_sys = audio_component->get_parameter_system();
    auto* transport = &audio_component->audio_transport;

    const uint32_t onsets[32]{
      15771, 34993, 44238, 54877, 68088, 74690, 83120, 94410, 102192, 107237, 114149,
      121055, 132979, 140573, 151761, 160537, 179416, 184906, 190785, 198069, 203866,
      209691, 217366, 228128, 236387, 247071, 265784, 274678, 304375, 312438, 336740,
      342887
    };

    (void) scale;
    (void) param_sys;

    auto buff = buffers->find_by_name("cajon.wav");
    auto buff_handle = buff ? buff.value() : AudioBufferHandle{};
    return new TransientsSampler1(node_id, transport, buff_store, buff_handle, onsets, 32);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "TransientsSampler1";
  return node;
}

DebugNode create_alt_reverb(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    return new AltReverbNode(node_id, audio_component->get_parameter_system());
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "AltReverb";
  return node;
}

DebugNode create_osc_swell(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [scale = audio_component->get_scale()](AudioNodeStorage::NodeID node_id) {
    return new OscSwell(node_id, scale, false);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "OscSwell";
  return node;
}

DebugNode create_simple_fm1(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto* scale = audio_component->get_scale();

  auto* param_sys = audio_component->get_parameter_system();
  auto node_ctor = [scale, param_sys](AudioNodeStorage::NodeID node_id) {
    return new SimpleFM1(node_id, param_sys, scale);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "SimpleFM1";
  return node;
}

DebugNode create_multi_component_sampler(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;

  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* param_sys = audio_component->get_parameter_system();
    auto* transport = &audio_component->audio_transport;

    const char* names[5] = {
      "piano-c.wav",
      "flute-c2.wav",
      "operator-c.wav",
      "choir-c.wav",
      "csv-pad.wav"
    };
    AudioBufferHandle buff_handles[5]{};
    int num_handles{};
    for (int i = 0; i < 5; i++) {
      if (auto handle = buffers->find_by_name(names[i])) {
        buff_handles[num_handles++] = handle.value();
      }
    }
    const uint32_t pss_group = get_ith_pitch_sample_group_id(*audio_component, 1);
    return new MultiComponentSampler(
      node_id, buff_store, buff_handles, num_handles, scale, transport, param_sys, pss_group);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "MultiComponentSampler";
  return node;
}

DebugNode create_chime_sampler(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;

  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* transport = &audio_component->audio_transport;
    auto* param_sys = audio_component->get_parameter_system();

    AudioBufferHandle bg_buff_handle{};
    if (auto buff = buffers->find_by_name("whitney_bird.wav")) {
      bg_buff_handle = buff.value();
    }

    AudioBufferHandle buff_handles[4]{};
    int num_handles{};
    const char* buff_names[4] = {"chime_c3.wav", "chime2_c3.wav", "piano-c.wav", "flute-c2.wav"};
    for (const char* name : buff_names) {
      if (auto buff = buffers->find_by_name(name)) {
        buff_handles[num_handles++] = buff.value();
      }
    }

    const uint32_t pss_group = get_ith_pitch_sample_group_id(*audio_component, 1);

    return new ChimeSampler(
      node_id, buff_store, scale, transport, param_sys, pss_group,
      bg_buff_handle, buff_handles, num_handles);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "ChimeSampler";
  return node;
}

DebugNode create_skittering1(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* transport = &audio_component->audio_transport;
    auto* param_sys = audio_component->get_parameter_system();

    AudioBufferHandle buff_handle{};
    if (auto buff = buffers->find_by_name("vocal_unison.wav")) {
      buff_handle = buff.value();
    }

    const uint32_t pss_group = get_ith_pitch_sample_group_id(*audio_component, 1);
    return new Skittering1(node_id, buff_store, transport, scale, param_sys, pss_group, buff_handle);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "Skittering1";
  return node;
}

DebugNode create_gauss_dist_pitches(const debug::DebugAudioNodesContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* param_sys = audio_component->get_parameter_system();
    return new GaussDistributedPitches1(node_id, scale, param_sys);
  };

  DebugNode node{};
  node.id = audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  node.name = "GaussDistributedPitches1";
  return node;
}

struct {
  std::vector<DebugNode> nodes;
} globals;

} //  anon

void debug::render_audio_nodes_gui(const DebugAudioNodesContext& context) {
  auto& node_storage = context.audio_component.audio_node_storage;
  auto* param_sys = context.audio_component.get_parameter_system();

  ImGui::Begin("Nodes");

  std::tuple<const char*, MakeNode*> make_nodes[32];
  int num_make_nodes{};
  make_nodes[num_make_nodes++] = {"DebugTuning", create_debug_tuning};
  make_nodes[num_make_nodes++] = {"TransientsSampler1", create_transients_sampler1};
  make_nodes[num_make_nodes++] = {"GaussDistributedPitches1", create_gauss_dist_pitches};
  make_nodes[num_make_nodes++] = {"Skittering1", create_skittering1};
  make_nodes[num_make_nodes++] = {"MultiComponentSampler", create_multi_component_sampler};
  make_nodes[num_make_nodes++] = {"ChimeSampler", create_chime_sampler};
  make_nodes[num_make_nodes++] = {"AltReverbNode", create_alt_reverb};
  make_nodes[num_make_nodes++] = {"SimpleFM1", create_simple_fm1};
  make_nodes[num_make_nodes++] = {"OscSwell", create_osc_swell};

  for (int i = 0; i < num_make_nodes; i++) {
    if (ImGui::Button(std::get<0>(make_nodes[i]))) {
      globals.nodes.emplace_back() = std::get<1>(make_nodes[i])(context);
    }
  }

  auto it = globals.nodes.begin();
  while (it != globals.nodes.end()) {
    auto& node = *it;
    auto sid = "node" + std::to_string(node.id) + " (" + node.name + ")";
    if (ImGui::TreeNode(sid.c_str())) {
      if (ImGui::Button("Delete")) {
        context.selected.selected_port_ids.clear();
        context.audio_component.audio_connection_manager.maybe_delete_node(node.id);
        it = globals.nodes.erase(it);
        ImGui::TreePop();
        continue;
      }

      if (ImGui::TreeNode("Params")) {
        Temporary<AudioParameterDescriptor, 1024> store_params;
        auto params = store_params.view_stack();
        node_storage.audio_parameter_descriptors(node.id, params);
        for (auto& p : params) {
          if (!p.is_editable()) {
            continue;
          }
          AudioParameterValue v = param_system::ui_get_set_value_or_default(param_sys, p);
          if (v.is_float()) {
            if (ImGui::SliderFloat(p.name, &v.data.f, p.min.f, p.max.f)) {
              param_system::ui_set_value_if_no_other_writer(param_sys, p.ids, v);
            }
          } else if (v.is_int()) {
            if (ImGui::SliderInt(p.name, &v.data.i, p.min.i, p.max.i)) {
              param_system::ui_set_value_if_no_other_writer(param_sys, p.ids, v);
            }
          }
        }

        ImGui::TreePop();
      }

      if (auto info = node_storage.get_port_info_for_node(node.id)) {
        for (int io = 0; io < 2; io++) {
          int id{};
          for (auto& port: info.value()) {
            bool accept = io == 0 ? port.descriptor.is_input() : port.descriptor.is_output();
            if (accept) {
              auto in_id = (io == 0 ? "Input" : "Output") + std::to_string(id++) + " ";
              auto dt_str = port.descriptor.data_type == AudioNodeStorage::DataType::Float ?
                "(float)" : port.descriptor.data_type == AudioNodeStorage::DataType::MIDIMessage ?
                "(midi)" : "(unknown)";
              auto opt_str = port.descriptor.is_optional() ? " (opt) " : "";
              in_id += dt_str;
              in_id += opt_str;
              if (port.connected()) {
                in_id += " (*)";
              }
              if (ImGui::SmallButton(in_id.c_str())) {
                context.selected.insert(port.id);
              }
              if (port.connected()) {
                ImGui::SameLine();
                auto str_id = "Disconnect" + std::to_string(port.id);
                if (ImGui::SmallButton(str_id.c_str())) {
                  context.audio_component.audio_connection_manager.maybe_disconnect(port);
                }
              }
            }
          }
        }
      }

      ImGui::TreePop();
    }

    ++it;
  }

  ImGui::End();
}

GROVE_NAMESPACE_END
