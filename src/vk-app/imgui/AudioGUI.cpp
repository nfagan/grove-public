#include "AudioGUI.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "../audio_core/note_sets.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_processors/SpectrumNode.hpp"
#include "../audio_observation/AudioObservation.hpp"
#include "grove/audio/audio_device.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioRenderBufferSystem.hpp"
#include "grove/audio/dft.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "./imgui.hpp"
#include <imgui/imgui.h>

#if GROVE_INCLUDE_IMPLOT
#include <implot.h>
#endif

GROVE_NAMESPACE_BEGIN

namespace {

constexpr auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

Optional<AudioDeviceInfo> render_device_info(const AudioComponent& component) {
  const auto devices = audio::enumerate_devices();
  Optional<AudioDeviceInfo> new_device;
  for (int i = 0; i < int(devices.size()); i++) {
    auto& device = devices[i];

    auto max_ins = device.max_num_input_channels;
    auto max_outs = device.max_num_output_channels;
    auto latency_in = device.default_low_input_latency * 1e3;
    auto latency_out = device.default_low_output_latency * 1e3;

    std::string label{"Use"};
    label += std::to_string(i);
    if (component.audio_core.audio_stream.get_stream_info()->output_device_index == i) {
      label += "(*)";
    }
    if (ImGui::Button(label.c_str())) {
      new_device = device;
    }
    ImGui::SameLine();
    ImGui::Text("(%d) %s\n\t%d In, %d Out\n\t%0.2fms In, %0.2fms Out",
                device.device_index, device.name.c_str(), max_ins, max_outs,
                latency_in, latency_out);
  }
  return new_device;
}

bool default_input_int(const char* name, int* v) {
  return ImGui::InputInt(name, v, 1, 100, enter_flag);
}

bool render_frame_info(AudioCore::FrameInfo* info) {
  if (default_input_int("FramesPerBuffer", &info->frames_per_buffer)) {
    info->frames_per_render_quantum = info->frames_per_buffer;
    return true;
  } else {
    return false;
  }
}

void render_stats(const AudioComponent& component, const AudioGUIRenderParams& params) {
  const auto& ui_param_manager = component.ui_audio_parameter_manager;
  ImGui::Text("Nodes: %d", component.audio_node_storage.num_audio_processor_nodes());
  ImGui::Text("NodeCtors: %d", component.audio_node_storage.num_audio_processor_node_ctors());
  ImGui::Text("SimplePlacedNodes: %d", params.node_placement.num_nodes());
  ImGui::Text("ParameterMonitors: %d", params.observation.parameter_monitor.num_nodes());
  ImGui::Text("ActiveUIParameters: %d", ui_param_manager.num_active_ui_parameters());
  ImGui::Text("PendingUIParameterEvents: %d", ui_param_manager.num_pending_events());
  ImGui::Text("PendingAudioEvents: %d", component.num_pending_audio_events());

  if (ImGui::TreeNode("PortPlacement")) {
    auto stats = params.port_placement.get_stats();
    ImGui::Text("Bounds: %d", stats.num_bounds);
    ImGui::Text("SelToPort: %d", stats.num_selectable_ids_to_port_ids);
    ImGui::Text("PortToSel: %d", stats.num_port_ids_to_selectable_ids);
    ImGui::Text("PathFindingPos: %d", stats.num_path_finding_positions);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("MonitorableParameterSystem")) {
    auto stats = param_system::get_stats(component.get_monitorable_parameter_system());
    ImGui::Text("Parameters: %d", stats.num_parameters);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("NodeSignalValueSystem")) {
    auto stats = audio::get_stats(component.get_node_signal_value_system());
    ImGui::Text("Values: %d", stats.num_values);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("QuantizedTriggeredNotes")) {
    auto stats = qtn::ui_get_stats(component.get_quantized_triggered_notes());
    ImGui::Text("NumUIPendingFeedback: %d", stats.num_ui_pending_feedback);
    ImGui::Text("MaxNumNoteMessages: %d", stats.max_num_note_messages);
    ImGui::Text("NumNoteFeedbacksCreated: %d", stats.num_note_feedbacks_created);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("MIDIMessageStreamSystem")) {
    auto stats = midi::ui_get_stats(component.get_midi_message_stream_system());
    ImGui::Text("NumStreams: %d", stats.num_streams);
    ImGui::Text("NumPendingSetSourceMask: %d", stats.num_pending_set_source_mask);
    ImGui::Text("MaxNumPendingMessages: %d", stats.max_num_pending_messages_across_streams);
    ImGui::Text("MaxNumFeedbackNoteOnsets: %d", stats.max_num_feedback_note_onsets);
    ImGui::TreePop();
  }
}

void render_event_system() {
  auto stats = audio_event_system::ui_get_stats();
  ImGui::Text("NumPackets: %d", int(stats.total_num_packets));
  ImGui::Text("TotalEventCapacity: %d", int(stats.total_event_capacity));
  ImGui::Text("MaxPacketCapacity: %d", int(stats.max_packet_capacity));
  ImGui::Text("LatestNumEventsRequired: %d", int(stats.latest_num_events_required));
  ImGui::Text("NumPendingEvents: %d", int(stats.num_pending_events));
  ImGui::Text("NumNewlyAcquiredEvents: %d", int(stats.num_newly_acquired_events));
  ImGui::Text("NumNewlyReadyEvents: %d", int(stats.num_newly_ready_events));
  ImGui::Text("Load: %0.3f", stats.load_factor);
  ImGui::Text("Util: %0.3f", stats.utilization);
}

void render_param_system(const AudioParameterSystem* param_sys) {
  auto stats = param_system::ui_get_stats(param_sys);
  ImGui::Text("NumNewlySetValues: %d", stats.num_newly_set_values);
  ImGui::Text("NumNewlyRevertedToBreakPoints: %d", stats.num_newly_reverted_to_break_points);
  ImGui::Text("NumNeedResynchronize: %d", stats.num_need_resynchronize);
  ImGui::Text("NumBreakPointSets: %d", stats.num_break_point_sets);
  ImGui::Text("NumBreakPointParams: %d", stats.num_break_point_parameters);
  ImGui::Text("TotalNumBreakPoints: %d", stats.total_num_break_points);
  ImGui::Text("NumUIValues: %d", stats.num_ui_values);
  ImGui::Text("NumControlledByUI: %d", stats.num_controlled_by_ui);
  ImGui::Text("WriteAccessAcquiredUIParameters: %d", stats.num_write_access_acquired_parameters);
}

int gather_floats(const audio_buffer_system::BufferView& buff,
                  TemporaryView<float>& store_samples) {
  const auto num_frames = int(buff.num_frames());
  const unsigned char* data = buff.data_ptr();

  float* samples = store_samples.require(num_frames * 2);
  for (int i = 0; i < num_frames; i++) {
    auto data0 = data + i * sizeof(float) * 2;
    auto data1 = data + i * sizeof(float) * 2 + sizeof(float);
    memcpy(samples + i * 2, data0, sizeof(float));
    memcpy(samples + i * 2 + 1, data1, sizeof(float));
  }

  return num_frames;
}

int gather_floats(const audio_buffer_system::BufferAwaitingEvent& rcv,
                  uint32_t target_node_id, TemporaryView<float>& store_samples) {
  if (rcv.type_tag == 1 && rcv.instance_id == target_node_id && rcv.buff.is_float2()) {
    return gather_floats(rcv.buff, store_samples);
  } else {
    return 0;
  }
}

void render_render_buffer_system() {
  auto stats = audio_buffer_system::ui_get_stats();
  ImGui::Text("NumAllocatorPages: %d", stats.num_allocator_pages);
  ImGui::Text("AllocatorPageSize: %d", int(stats.allocator_page_size_bytes));
  ImGui::Text("NumAllocatedBytes: %d", int(stats.num_allocated_bytes));
  ImGui::Text("NumReservedBytes: %d", int(stats.num_reserved_bytes));
  ImGui::Text("MaxBytesAllocatedInEpoch: %d", int(stats.max_bytes_allocated_in_epoch));
  ImGui::Text("MaxBytesRequestedInEpoch: %d", int(stats.max_bytes_requested_in_epoch));

  ImGui::Text("NumReceivedBuffers: %d", int(stats.num_received_buffers));
  ImGui::Text("NumPendingFree: %d", int(stats.num_pending_free));
}

bool is_spectrum_node(uint32_t node_id, const AudioNodeStorage& node_storage) {
  if (node_storage.node_exists(node_id) && node_storage.is_instance_created(node_id)) {
    if (auto* base = node_storage.get_audio_processor_node_instance(node_id)) {
      return dynamic_cast<const SpectrumNode*>(base) != nullptr;
    }
  }
  return false;
}

void render_spectrum(AudioGUI& gui, const AudioComponent&) {
#if GROVE_INCLUDE_IMPLOT
  Temporary<float, 1024> store_floats;
  auto store_float_view = store_floats.view();
  int num_spectral_frames{};

  if (gui.selected_spectrum_node) {
    uint32_t target_node = gui.selected_spectrum_node.value();
    auto newly_received = audio_buffer_system::ui_read_newly_received();
    for (auto& rcv : newly_received) {
      num_spectral_frames = gather_floats(rcv, target_node, store_float_view);
      if (num_spectral_frames > 0) {
        break;
      }
    }
  }

  if (num_spectral_frames > 0) {
    if (int(gui.spectrum_data.size()) < num_spectral_frames) {
      gui.spectrum_data.resize(num_spectral_frames);
    }

    float* moduli = gui.spectrum_data.data();
    complex_moduli(store_float_view.stack, moduli, num_spectral_frames);
    for (int i = 0; i < num_spectral_frames; i++) {
      moduli[i] = float(amplitude_to_db(moduli[i]));
    }
  }

  if (!gui.spectrum_data.empty()) {
    ImGui::Begin("Spectrum");

    if (ImPlot::BeginPlot("Spectrum")) {
//      ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
      ImPlot::PlotLine("Spectrum", gui.spectrum_data.data(), int(gui.spectrum_data.size() / 2));
      ImPlot::EndPlot();
    }

    ImGui::End();
  }
#else
  (void) gui;
#endif
}

void render_arp_system(const AudioComponent& component) {
  auto* sys = component.get_arpeggiator_system();
  int num_arps = arp::ui_get_num_instances(sys);
  for (int i = 0; i < num_arps; i++) {
    auto label = std::string{"Arp"} + std::to_string(i);
    if (ImGui::TreeNode(label.c_str())) {
      const ArpeggiatorInstanceHandle inst = arp::ui_get_ith_instance(sys, i);
      ReadArpeggiatorState read_state = arp::ui_read_state(sys, inst);
      int curr_pitch_mode = int(read_state.pitch_mode);
      if (ImGui::SliderInt("PitchMode", &curr_pitch_mode, 0, 3)) {
        arp::ui_set_pitch_mode(sys, inst, ArpeggiatorSystemPitchMode(curr_pitch_mode));
      }
      int curr_dur_mode = int(read_state.duration_mode);
      if (ImGui::SliderInt("DurationMode", &curr_dur_mode, 0, 3)) {
        arp::ui_set_duration_mode(sys, inst, ArpeggiatorSystemDurationMode(curr_dur_mode));
      }
      if (ImGui::SliderInt("NumActiveSlots", &read_state.num_slots_active, 1, 4)) {
        arp::ui_set_num_active_slots(sys, inst, uint8_t(read_state.num_slots_active));
      }
      ImGui::TreePop();
    }
  }
}

void render_pitch_sampling_system(const AudioComponent& component) {
  auto* sys = component.get_pitch_sampling_system();
  int num_groups = pss::ui_get_num_groups(sys);
  for (int i = 0; i < num_groups; i++) {
    const PitchSampleSetGroupHandle group = pss::ui_get_ith_group(sys, i);
    int ns = pss::ui_get_num_sets_in_group(sys, group);
    ns = std::min(ns, 1);
    for (int j = 0; j < ns; j++) {
      std::string id{"group"};
      id += std::to_string(i) + "-" + std::to_string(j);
      if (ImGui::TreeNode(id.c_str())) {

        int num_notes{};
        float sts[notes::max_num_notes];

        bool pref_triggered = pss::ui_prefers_triggered_sample_set(sys, group, j);
        if (ImGui::Checkbox("PreferTriggered", &pref_triggered)) {
          pss::ui_set_prefer_triggered_sample_set(sys, group, j, pref_triggered);
        }
        if (ImGui::Button("NoteSet0")) {
          num_notes = notes::ui_get_note_set0(sts);
        }
        if (ImGui::Button("NoteSet1")) {
          num_notes = notes::ui_get_note_set1(sts);
        }
        if (ImGui::Button("NoteSet2")) {
          num_notes = notes::ui_get_note_set2(sts);
        }
        if (ImGui::Button("NoteSet3")) {
          num_notes = notes::ui_get_note_set3(sts);
        }

        if (num_notes > 0) {
          pss::ui_set_sample_set_from_semitones(sys, group, uint32_t(j), sts, num_notes);
        }
        ImGui::TreePop();
      }
    }
  }
}

void render_audio_scale_system(const AudioComponent& component) {
  auto* scale_sys = component.get_audio_scale_system();

  const auto scale_descs = scale_system::ui_get_active_scale_descriptors(scale_sys);
  ImGui::Text("Scale0: %s (%d)", scale_descs.scales[0].name, scale_descs.scales[0].index);
  ImGui::Text("Scale1: %s (%d)", scale_descs.scales[1].name, scale_descs.scales[1].index);

  float frac_scale1 = scale_system::ui_get_frac_scale1(scale_sys);
  if (ImGui::SliderFloat("FracScale1", &frac_scale1, 0.0f, 1.0f)) {
    scale_system::ui_set_frac_scale1(scale_sys, frac_scale1);
  }

  int scale0_index = scale_descs.scales[0].index;
  int scale1_index = scale_descs.scales[1].index;
  bool scales_modified{};

  if (ImGui::TreeNode("Scales")) {
    const int ns = scale_system::ui_get_num_scales(scale_sys);
    for (int i = 0; i < ns; i++) {
      auto scale_desc = scale_system::ui_get_ith_scale_desc(scale_sys, i);
      ImGui::Text("Scale: %s (%d)", scale_desc.name, scale_desc.index);
      std::string button_text0{"use0-"};
      button_text0 += std::to_string(i);
      std::string button_text1{"use1-"};
      button_text1 += std::to_string(i);
      ImGui::SameLine();
      if (ImGui::SmallButton(button_text0.c_str())) {
        scale0_index = scale_desc.index;
        scales_modified = true;
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(button_text1.c_str())) {
        scale1_index = scale_desc.index;
        scales_modified = true;
      }
    };
    ImGui::TreePop();
  }

  if (scales_modified) {
    scale_system::ui_set_scale_indices(scale_sys, scale0_index, scale1_index);
  }

  Tuning tuning = *scale_system::ui_get_tuning(scale_sys);
  bool tuning_modified{};
  int ref_st = int(tuning.reference_semitone);
  if (default_input_int("ReferenceSemitone", &ref_st)) {
    tuning.reference_semitone = ref_st;
    tuning_modified = true;
  }

  auto ref_freq = int(tuning.reference_frequency);
  if (default_input_int("ReferenceFrequency", &ref_freq) &&
      ref_freq > 0 && ref_freq < 4096) {
    tuning.reference_frequency = ref_freq;
    tuning_modified = true;
  }

  if (tuning_modified) {
    scale_system::ui_set_tuning(scale_sys, tuning);
  }
}

} //  anon

AudioGUIUpdateResult AudioGUI::render(const AudioComponent& component,
                                      const AudioGUIRenderParams& params) {
  AudioGUIUpdateResult result;

  if (params.selected_node_id) {
    if (is_spectrum_node(params.selected_node_id.value(), component.audio_node_storage)) {
      selected_spectrum_node = params.selected_node_id.value();
    }
  }
  if (selected_spectrum_node) {
    if (!is_spectrum_node(selected_spectrum_node.value(), component.audio_node_storage)) {
      selected_spectrum_node = NullOpt{};
    }
  }

  ImGui::Begin("AudioGUI");

  if (stopwatch.delta().count() >= 100e-3) {
    cpu_load = float(component.audio_core.audio_stream.get_stream_load()) * 100.0f;
    stopwatch.reset();
  }
  ImGui::Text("Load: %0.2f", cpu_load);

  if (ImGui::TreeNode("Device")) {
    if (auto device = render_device_info(component)) {
      result.change_device = device.value();
    }
    auto frame_info = component.audio_core.get_frame_info();
    if (render_frame_info(&frame_info)) {
      result.new_frame_info = frame_info;
    }

    bool stream_started = component.audio_core.audio_stream.is_stream_started();
    if (ImGui::Button(stream_started ? "StopStream" : "StartStream")) {
      result.toggle_stream_started = true;
    }

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Stats")) {
    render_stats(component, params);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("AudioEventSystem")) {
    render_event_system();
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("AudioParameterSystem")) {
    render_param_system(component.get_parameter_system());
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("AudioRenderBufferSystem")) {
    render_render_buffer_system();
    ImGui::TreePop();
  }

  const auto* tuning = component.ui_audio_scale.get_tuning();
  auto ref_st = int(tuning->reference_semitone);
  if (default_input_int("ReferenceSemitone", &ref_st)) {
    auto new_tuning = *tuning;
    new_tuning.reference_semitone = ref_st;
    result.tuning = new_tuning;
  }

  auto st_per_oct = int(tuning->semitones_per_octave);
  if (default_input_int("SemitonesPerOctave", &st_per_oct) &&
      st_per_oct > 0 &&
      st_per_oct <= 255) {
    auto new_tuning = *tuning;
    new_tuning.semitones_per_octave = uint8_t(st_per_oct);
    result.tuning = new_tuning;
  }

  auto ref_freq = int(tuning->reference_frequency);
  if (default_input_int("ReferenceFrequency", &ref_freq) &&
      ref_freq > 0 && ref_freq < 4096) {
    auto new_tuning = *tuning;
    new_tuning.reference_frequency = ref_freq;
    result.tuning = new_tuning;
  }

  auto bpm = float(component.audio_transport.get_bpm());
  if (ImGui::InputFloat("BPM", &bpm, 0.0f, 0.0f, "%0.2f", enter_flag)) {
    result.new_bpm = double(std::floor(bpm * 2.0f) / 2.0f);
  }

  bool tuning_controlled_by_env = params.tuning_controlled_by_environment;
  if (ImGui::Checkbox("TuningControlledByEnvironment", &tuning_controlled_by_env)) {
    result.tuning_controlled_by_environment = tuning_controlled_by_env;
  }

  bool metronome_enabled = metronome::ui_is_enabled(component.get_metronome());
  if (ImGui::Checkbox("MetronomeEnabled", &metronome_enabled)) {
    result.metronome_enabled = metronome_enabled;
  }

  if (ImGui::TreeNode("ArpeggiatorSystem")) {
    render_arp_system(component);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("PitchSamplingSystem")) {
    render_pitch_sampling_system(component);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("AudioScaleSystem")) {
    render_audio_scale_system(component);
    ImGui::TreePop();
  }

  ImGui::Checkbox("ShowSpectrum", &show_spectrum);

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();

  if (show_spectrum) {
    render_spectrum(*this, component);
  }

  return result;
}

GROVE_NAMESPACE_END
