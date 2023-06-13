#include "AudioComponent.hpp"
#include "UITrackSystem.hpp"
#include "grove/audio/audio_config.hpp"
#include "grove/audio/io.hpp"
#include "grove/audio/AudioRenderBufferSystem.hpp"
#include "grove/audio/QuantizedTriggeredNotes.hpp"
#include "grove/audio/NoteClipStateMachineSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/vector_util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr const char* logging_id() {
  return "update_audio_core";
}

void initialize_audio_buffer_store(AudioComponent& component) {
  auto* buffer_store = component.get_audio_buffer_store();
  auto& ui_buffer_store = component.ui_audio_buffer_store;
  const auto default_files = AudioBuffers::default_audio_buffer_file_names();

  for (auto& file : default_files) {
    auto full_file = AudioBuffers::audio_buffer_full_path(file.c_str());
    const bool normalize = true;
    const bool max_normalize = true;
    auto load_result = io::read_wav_as_float(full_file.c_str(), normalize, max_normalize);

    if (load_result.success) {
      auto future = buffer_store->ui_add_in_memory(
        load_result.descriptor, std::move(load_result.data));

      auto cb = [&component, file](auto&& handle) {
        component.audio_buffers.push(file, handle);
      };

      ui_buffer_store.on_buffer_available(std::move(future), std::move(cb));
    }
  }

  int num_no_max_norm{};
  const char** files = AudioBuffers::addtl_audio_buffer_file_names_no_max_norm(&num_no_max_norm);
  for (int i = 0; i < num_no_max_norm; i++) {
    (void) component.simple_load_wav_audio_buffer(files[i]);
  }
}

bool record_state_transition(AudioComponent& component,
                             UIAudioRecordStream::State state,
                             AudioRecordStreamHandle handle) {
  if (state == UIAudioRecordStream::State::ArmRecord) {
    return component.ui_audio_graph_destination_nodes.arm_record(
      &component.audio_core.audio_recorder, handle);

  } else if (state == UIAudioRecordStream::State::Recording) {
    return false;

  } else {
    return true;
  }
}

UIAudioRecordStream::UpdateResult update_recording(AudioComponent& component) {
  auto recorder = &component.audio_core.audio_recorder;
  return component.ui_audio_record_stream.update(recorder, {
    [&component](auto&& state, auto&& handle) {
      return record_state_transition(component, state, handle);
    }
  });
}

bool maybe_initiate_recording(AudioComponent& component) {
  if (component.ui_audio_record_stream.is_idle()) {
    auto channel_types = component.ui_audio_graph_destination_nodes.record_channel_types();
    AudioRecordChannelSet layout{};
    for (auto& type : channel_types) {
      layout.add(type);
    }
    layout.finalize();

    return component.ui_audio_record_stream.create(
      &component.audio_core.audio_recorder, std::move(layout), &component.audio_transport);

  } else {
    return false;
  }
}

void update_parameter_system(AudioParameterSystem* sys, const AudioComponent::UpdateResult& res) {
  const auto& new_connects = res.connection_update_result.new_connections;
  Temporary<AudioNodeStorage::NodeID, 1024> connected_node_ids;
  AudioNodeStorage::NodeID* ids = connected_node_ids.require(int(new_connects.size()) * 2);

  int count{};
  for (auto& connect : new_connects) {
    ids[count++] = connect.first.node_id;
    ids[count++] = connect.second.node_id;
  }

  param_system::ui_end_update(sys, {
    res.event_update_result.any_event_system_dropped_events,
    ArrayView<AudioNodeStorage::NodeID>{ids, ids + count},
    res.connection_update_result.new_node_deletions
  });
}

using ConnectResult = AudioConnectionManager::UpdateResult;
void on_audio_connection_update(AudioComponent& component, const ConnectResult& result) {
  auto& dest_nodes = component.ui_audio_graph_destination_nodes;
  auto* param_sys = component.get_parameter_system();
  auto& ui_param_manager = *component.get_ui_parameter_manager();

  for (auto& del : result.new_node_deletions) {
    if (auto node = dest_nodes.delete_node(del, param_sys, ui_param_manager)) {
      component.audio_graph_component.renderer.delete_destination(node);
    }
  }

  param_system::ui_evaluate_deleted_nodes(
    component.get_simple_set_parameter_system(), result.new_node_deletions);
}

void on_new_audio_record_stream_data(AudioComponent& component,
                                     AudioRecordStreamResult&& result,
                                     UIAudioBufferStore::OnBufferAvailable callback) {
  auto* buffer_store = component.get_audio_buffer_store();
  auto& ui_buffer_store = component.ui_audio_buffer_store;
  auto descriptor = AudioBufferDescriptor::from_audio_record_stream_result(result);
  auto future = buffer_store->ui_add_in_memory(descriptor, std::move(result.data));
  ui_buffer_store.on_buffer_available(std::move(future), std::move(callback));
}

} //  anon

AudioComponent::AudioComponent() :
  triggered_buffer_renderer{audio_core.renderer.get_audio_buffer_store()},
  audio_connection_manager{&audio_node_storage, &audio_graph_component.graph_proxy} {
  //
  monitorable_parameter_system = param_system::get_global_monitorable_parameter_system();
  node_signal_value_system = audio::get_global_node_signal_value_system();
}

void AudioComponent::terminate() {
  audio_core.terminate();
  audio::ui_terminate_events(event_update_context);
  audio_buffer_system::ui_terminate();
}

AudioComponentGUI::UpdateResult
AudioComponent::render_gui(IMGUIWrapper& wrapper,
                           const SelectedInstrumentComponents& selected_components) {
  return gui.render_gui(*this, selected_components, wrapper);
}

void AudioComponent::initialize(bool initialize_default_audio_stream, int desired_num_frames) {
  audio_core.initialize(initialize_default_audio_stream, desired_num_frames);
  global_attenuator->set_gain(-10.0);
  
  audio_core.push_render_modification(
    AudioCore::make_add_audio_effect_modification(spectrum_analyzer.get()));
  audio_core.push_render_modification(
    AudioCore::make_add_audio_effect_modification(global_attenuator.get()));
  audio_core.push_render_modification(
    AudioCore::make_add_transport_modification(&audio_transport));
  audio_core.push_render_modification(
    AudioCore::make_add_scale_modification(&audio_scale));

  auto graph_init_res = audio_graph_component.initialize();
  for (auto& mod : graph_init_res.render_modifications) {
    audio_core.push_render_modification(mod);
  }

  initialize_audio_buffer_store(*this);

  audio_core.push_render_modification(
    AudioCore::make_add_renderable_modification(&triggered_buffer_renderer));

  audio::ui_initialize_events(event_update_context);

  //  note clip system init
  grove::initialize(&note_clip_system);
  audio_core.push_render_modification(
    AudioCore::make_add_note_clip_system_modification(&note_clip_system));

  //  midi message stream system init
  midi::ui_initialize(get_midi_message_stream_system());

  //  qtn init
  qtn::ui_initialize(get_quantized_triggered_notes(), &audio_transport);

  //  arp init
  arp::ui_initialize(
    get_arpeggiator_system(), get_midi_message_stream_system(),
    get_pitch_sampling_system(), &audio_transport);

  //  timeline system init
  ui_timeline_system.initialize(
    timeline_system, note_clip_system,
    *get_midi_message_stream_system(), &audio_transport, get_audio_buffer_store());
  notes::ui_initialize(get_triggered_notes(), &audio_transport);

  audio_core.push_render_modification(
    AudioCore::make_add_timeline_system_modification(&timeline_system));

  //  audio parameter system init
  param_system::ui_initialize(get_parameter_system(), &audio_transport);
  param_system::ui_initialize(
    get_simple_set_parameter_system(), &audio_node_storage, get_parameter_system());

  //  audio node isolator
  ni::ui_init_audio_node_isolator(get_audio_node_isolator(), &audio_graph_component.renderer);

  //  pitch sampling system
  pss::ui_initialize(get_pitch_sampling_system());

  //  audio scale system
  scale_system::ui_initialize(get_audio_scale_system());

  //  note clip state machine system
  ncsm::ui_initialize(
    get_note_clip_state_machine_system(),
    &audio_transport, &note_clip_system, get_midi_message_stream_system());

  //  new metronome
  metronome::ui_initialize(get_metronome(), &audio_transport);
}

AudioComponent::UpdateResult AudioComponent::ui_begin_update(const UpdateInfo& update_info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("AudioComponent/update");

  UpdateResult result;
  const auto& audio_stream = audio_core.audio_stream;
  audio_core.ui_update();

  if (audio_core.renderer.check_output_buffer_underflow() ||
      audio_core.renderer.get_cpu_usage_estimate() >= 1.0) {
    GROVE_LOG_SEVERE_CAPTURE_META("Audio output underflow.", logging_id());
  }

  if (!enabled_audio_events) {
    audio_core.renderer.enable_main_thread_events();
    enabled_audio_events = true;
  }

  const auto is_stream_started = audio_stream.is_stream_started();

  result.event_update_result = audio::ui_process_events(event_update_context, {
    is_stream_started,
    audio_core,
    spectrum_analyzer.get(),
    update_info.spectrum_analyzer_frame_callback
  });

  if (is_stream_started) {
    const auto current_time = audio_stream.current_time();
    const auto sample_rate = audio_stream.get_stream_info()->sample_rate;
    ui_audio_parameter_manager.update(
      event_update_context.ui_parameter_change_list, current_time, sample_rate);
  }

  audio_buffer_system::ui_update(
    make_view(event_update_context.new_render_buffer_event_ids),
    result.event_update_result.any_event_system_dropped_events);

  audio_graph_component.update(audio_core.get_frame_info().frames_per_buffer);
  ui_audio_buffer_store.update();
  triggered_buffer_renderer.ui_update();

  param_system::update_monitorable_parameter_values(
    monitorable_parameter_system,
    audio_node_storage, ui_audio_parameter_manager, update_info.real_dt);

  audio::update_node_signal_values(node_signal_value_system, audio_node_storage);

  auto record_update_res = update_recording(*this);
  if (record_update_res.record_result) {
    //  New data available.
    on_new_audio_record_stream_data(
      *this, std::move(record_update_res.record_result.value()), nullptr);
  }

  result.node_isolator_update_result = ni::ui_update(get_audio_node_isolator());
  result.connection_update_result = audio_connection_manager.update();
  on_audio_connection_update(*this, result.connection_update_result);

  auto qtn_update_res = qtn::ui_update(get_quantized_triggered_notes());
  (void) qtn_update_res;

  result.midi_message_stream_update_result = midi::ui_update(get_midi_message_stream_system());
  arp::ui_update(get_arpeggiator_system());
  pss::ui_update(get_pitch_sampling_system());
  ncsm::ui_update(get_note_clip_state_machine_system());
  scale_system::ui_update(get_audio_scale_system());

  return result;
}

void AudioComponent::ui_end_update(double real_dt, const UpdateResult& res) {
  ui_audio_scale.update(&audio_scale);
  ui_update(&note_clip_system);
  notes::ui_update(get_triggered_notes(), real_dt);
  ui_timeline_system.end_update(timeline_system);
  track::end_update(track::get_global_ui_track_system(), *this);
  update_parameter_system(get_parameter_system(), res);
  audio_transport.ui_update();
}

bool AudioComponent::initiate_recording() {
  return maybe_initiate_recording(*this);
}

void AudioComponent::add_pending_audio_buffer(audio::PendingAudioBufferAvailable pend) {
  auto fut = get_audio_buffer_store()->ui_add_in_memory(pend.descriptor, std::move(pend.data));
  ui_audio_buffer_store.on_buffer_available(std::move(fut), std::move(pend.callback));
}

bool AudioComponent::simple_load_wav_audio_buffer(const char* name) {
  auto idle_file = AudioBuffers::audio_buffer_full_path(name);
  auto idle_load_result = io::read_wav_as_float(idle_file.c_str());
  if (idle_load_result.success) {
    audio::PendingAudioBufferAvailable pend{};
    pend.descriptor = std::move(idle_load_result.descriptor);
    pend.data = std::move(idle_load_result.data);
    pend.callback = [name, buffs = &audio_buffers](AudioBufferHandle handle) {
      buffs->push(name, handle);
    };
    add_pending_audio_buffer(std::move(pend));
    return true;
  } else {
    return false;
  }
}

GROVE_NAMESPACE_END