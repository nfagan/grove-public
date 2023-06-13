#pragma once

#include "events.hpp"
#include "common.hpp"
#include "AudioNodeStorage.hpp"
#include "UIAudioParameterManager.hpp"
#include "UIAudioBufferStore.hpp"
#include "UIAudioGraphDestinationNodes.hpp"
#include "UIAudioRecordStream.hpp"
#include "UIAudioScale.hpp"
#include "UITimelineSystem.hpp"
#include "UIMIDIMessageStreamNodes.hpp"
#include "AudioBuffers.hpp"
#include "AudioGraphComponent.hpp"
#include "AudioConnectionManager.hpp"
#include "SimpleSetParameterSystem.hpp"
#include "MonitorableParameterSystem.hpp"
#include "NodeSignalValueSystem.hpp"
#include "AudioComponentGUI.hpp"
#include "grove/audio/AudioCore.hpp"
#include "grove/audio/audio_effects/SpectrumAnalyzer.hpp"
#include "grove/audio/audio_effects/UtilityEffect.hpp"
#include "grove/audio/TriggeredBufferRenderer.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/NoteClipSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioNodeIsolator.hpp"
#include "grove/audio/QuantizedTriggeredNotes.hpp"
#include "grove/audio/MIDIMessageStreamSystem.hpp"
#include "grove/audio/ArpeggiatorSystem.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/NoteClipStateMachineSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/audio/Metronome.hpp"
#include <string>

namespace grove {

class AudioComponent {
public:
  using SpectrumAnalyzerFrameCallback =
    std::function<void(const SpectrumAnalyzer::AnalysisFrame& frame)>;

  struct UpdateInfo {
    double real_dt;
    SpectrumAnalyzerFrameCallback spectrum_analyzer_frame_callback{};
  };

  struct UpdateResult {
    AudioConnectionManager::UpdateResult connection_update_result;
    audio::EventUpdateResult event_update_result{};
    ni::AudioNodeIsolatorUpdateResult node_isolator_update_result{};
    MIDIMessageStreamSystemUpdateResult midi_message_stream_update_result;
  };

public:
  AudioComponent();

  UpdateResult ui_begin_update(const UpdateInfo& update_info);
  void ui_end_update(double real_dt, const UpdateResult& update_res);
  void initialize(bool init_default_stream, int desired_num_frames = -1);
  void terminate();

  AudioComponentGUI::UpdateResult
  render_gui(IMGUIWrapper& wrapper, const SelectedInstrumentComponents& selected_components);

  int num_pending_audio_events() const {
    return int(event_update_context.pending_audio_events.size());
  }

  AudioBufferStore* get_audio_buffer_store() {
    return audio_core.renderer.get_audio_buffer_store();
  }
  const AudioBufferStore* get_audio_buffer_store() const {
    return audio_core.renderer.get_audio_buffer_store();
  }
  const AudioScale* get_scale() const {
    return &audio_scale;
  }
  UIAudioScale* get_ui_scale() {
    return &ui_audio_scale;
  }
  AudioParameterSystem* get_parameter_system() const {
    return param_system::get_global_audio_parameter_system();
  }
  UIAudioParameterManager* get_ui_parameter_manager() {
    return &ui_audio_parameter_manager;
  }
  UITimelineSystem* get_ui_timeline_system() {
    return &ui_timeline_system;
  }
  TimelineSystem* get_timeline_system() {
    return &timeline_system;
  }
  param_system::MonitorableParameterSystem* get_monitorable_parameter_system() const {
    return monitorable_parameter_system;
  }
  SimpleSetParameterSystem* get_simple_set_parameter_system() const {
    return param_system::get_global_simple_set_parameter_system();
  }
  audio::NodeSignalValueSystem* get_node_signal_value_system() const {
    return node_signal_value_system;
  }
  AudioNodeIsolator* get_audio_node_isolator() const {
    return ni::get_global_audio_node_isolator();
  }
  QuantizedTriggeredNotes* get_quantized_triggered_notes() const {
    return qtn::get_global_quantized_triggered_notes();
  }
  MIDIMessageStreamSystem* get_midi_message_stream_system() const {
    return midi::get_global_midi_message_stream_system();
  }
  ArpeggiatorSystem* get_arpeggiator_system() const {
    return arp::get_global_arpeggiator_system();
  }
  PitchSamplingSystem* get_pitch_sampling_system() const {
    return pss::get_global_pitch_sampling_system();
  }
  NoteClipStateMachineSystem* get_note_clip_state_machine_system() const {
    return ncsm::get_global_note_clip_state_machine();
  }
  NoteClipSystem* get_note_clip_system() {
    return &note_clip_system;
  }
  UIMIDIMessageStreamNodes* get_ui_midi_message_stream_nodes() {
    return &ui_midi_message_stream_nodes;
  }
  TriggeredNotes* get_triggered_notes() const {
    return notes::get_global_triggered_notes();
  }
  metronome::Metronome* get_metronome() const {
    return metronome::get_global_metronome();
  }
  AudioScaleSystem* get_audio_scale_system() const {
    return scale_system::get_global_audio_scale_system();
  }

  bool initiate_recording();
  void add_pending_audio_buffer(audio::PendingAudioBufferAvailable pend);
  bool simple_load_wav_audio_buffer(const char* name);

public:
  AudioCore audio_core;
  Transport audio_transport;
  AudioNodeStorage audio_node_storage;

  UIAudioParameterManager ui_audio_parameter_manager;
  param_system::MonitorableParameterSystem* monitorable_parameter_system{};
  audio::NodeSignalValueSystem* node_signal_value_system{};
  UIAudioBufferStore ui_audio_buffer_store;
  UIAudioGraphDestinationNodes ui_audio_graph_destination_nodes;
  UIAudioRecordStream ui_audio_record_stream;
  UIMIDIMessageStreamNodes ui_midi_message_stream_nodes;

  NoteClipSystem note_clip_system;
  TimelineSystem timeline_system;
  UITimelineSystem ui_timeline_system;

  AudioScale audio_scale{default_tuning()};
  UIAudioScale ui_audio_scale{default_tuning()};

public:
  audio::EventUpdateContext event_update_context;
  bool enabled_audio_events{false};

  std::unique_ptr<SpectrumAnalyzer> spectrum_analyzer{std::make_unique<SpectrumAnalyzer>()};
  std::unique_ptr<UtilityEffect> global_attenuator{std::make_unique<UtilityEffect>()};

  TriggeredBufferRenderer triggered_buffer_renderer;
  AudioGraphComponent audio_graph_component;
  AudioConnectionManager audio_connection_manager;

  AudioBuffers audio_buffers;

  AudioComponentGUI gui;
};

}