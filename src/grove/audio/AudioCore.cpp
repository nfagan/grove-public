#include "AudioCore.hpp"
#include "fdft.hpp"
#include "audio_device.hpp"
#include "audio_callback.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

bool is_valid_frame_info(const AudioCore::FrameInfo& frame_info) {
  const auto is_pow2 = [](int v) -> bool {
    return v > 0 && (v & (v - 1)) == 0;
  };

  if (!is_pow2(frame_info.frames_per_render_quantum) ||
      !is_pow2(frame_info.frames_per_buffer)) {
    return false;
  }

#if GROVE_RENDER_AUDIO_IN_CALLBACK
  if (frame_info.frames_per_buffer != frame_info.frames_per_render_quantum) {
    return false;
  }
#endif

  return true;
}

bool process_renderer_modification(const AudioRenderer::Accessors& accessors,
                                   AudioRenderer::Modification& mod) {
  bool all_processed{true};
  const auto maybe_process = [&all_processed](auto& mod, auto* accessor) -> void {
    bool processed{true};

    if (mod.has_value()) {
      if (mod.remove) {
        processed = accessor->writer_remove(mod.value);
      } else {
        processed = accessor->writer_add(mod.value);
      }
    }

    if (processed) {
      mod.value = {};
    } else {
      all_processed = false;
    }
  };

  maybe_process(mod.renderable, accessors.renderables);
  maybe_process(mod.transport, accessors.transports);
  maybe_process(mod.scale, accessors.scales);
  maybe_process(mod.recorder, accessors.recorders);
  maybe_process(mod.audio_effect, accessors.effects);
  maybe_process(mod.timeline_system, accessors.timeline_systems);
  maybe_process(mod.note_clip_system, accessors.note_clip_systems);

  return all_processed;
}

void update_accessors(const AudioRenderer::Accessors& accessors) {
  (void) accessors.renderables->writer_update();
  (void) accessors.transports->writer_update();
  (void) accessors.scales->writer_update();
  (void) accessors.recorders->writer_update();
  (void) accessors.effects->writer_update();
  (void) accessors.timeline_systems->writer_update();
  (void) accessors.note_clip_systems->writer_update();
}

} //  anon

/*
 * AudioCore
 */

AudioCore::AudioCore() {
  push_render_modification(make_add_recorder_modification(&audio_recorder));
}

AudioCore::~AudioCore() {
  terminate();
}

void AudioCore::terminate() {
  audio_recorder.terminate();
  audio_stream.terminate();
  audio_thread.stop();
  terminate_port_audio();
}

AudioCore::FrameInfo AudioCore::get_frame_info() const {
  FrameInfo result{};
  result.frames_per_render_quantum = frames_per_render_quantum;
  result.frames_per_buffer = frames_per_buffer;
  return result;
}

void AudioCore::toggle_stream_started() {
  if (audio_stream.is_stream_started()) {
    audio_stream.stop();
  } else {
    audio_stream.start();
  }
}

bool AudioCore::change_stream(const AudioDeviceInfo& input_device,
                              const AudioDeviceInfo& output_device,
                              const FrameInfo& frame_info) {
  if (!is_valid_frame_info(frame_info)) {
    return false;
  }

  if (audio_stream.is_stream_open()) {
    auto close_status = audio_stream.close();
    if (!close_status.success) {
      return false;
    }
  }

  frames_per_buffer = frame_info.frames_per_buffer;
  frames_per_render_quantum = frame_info.frames_per_render_quantum;

  auto input_params = AudioStream::Parameters::from_device_info(
    input_device, num_input_channels, sample_format);
  auto output_params = AudioStream::Parameters::from_device_info(
    output_device, num_output_channels, sample_format);
  auto open_status = audio_stream.open(
    input_params,
    output_params,
    sample_rate,
    frames_per_buffer,
    frames_per_render_quantum,
    &audio::callback,
    this);

  if (!open_status.success) {
    return false;
  }

  auto start_status = audio_stream.start();
  if (!start_status.success) {
    return false;
  }

  return true;
}

bool AudioCore::change_stream(const AudioDeviceInfo& target_device, const FrameInfo& frame_info) {
  return change_stream(target_device, target_device, frame_info);
}

bool AudioCore::change_stream(const FrameInfo& frame_info) {
  if (!audio_stream.is_stream_started()) {
    return false;
  }
  const auto devices = audio::enumerate_devices();
  auto* stream_info = audio_stream.get_stream_info();
  return change_stream(
    devices[stream_info->input_device_index],
    devices[stream_info->output_device_index],
    frame_info);
}

bool AudioCore::change_stream(const AudioDeviceInfo& target_device) {
  return change_stream(target_device, target_device, get_frame_info());
}

void AudioCore::initialize(bool start_default_stream, int desired_frames) {
  init_fdft();

  if (desired_frames > 0) {
    FrameInfo frame_info{};
    frame_info.frames_per_render_quantum = desired_frames;
    frame_info.frames_per_buffer = desired_frames;
    if (is_valid_frame_info(frame_info)) {
      frames_per_buffer = desired_frames;
      frames_per_render_quantum = desired_frames;
    }
  }

  if (initialize_port_audio()) {
#if !GROVE_RENDER_AUDIO_IN_CALLBACK
    GROVE_LOG_INFO_CAPTURE_META("Rendering audio in new thread.", "AudioCore");
    audio_thread.start();
#else
    GROVE_LOG_INFO_CAPTURE_META("Rendering audio in audio callback.", "AudioCore");
#endif
    audio_recorder.initialize();
    if (start_default_stream) {
      auto open_status = audio_stream.open_asio_or_default(
        num_output_channels,
        sample_format,
        sample_rate,
        frames_per_buffer,
        frames_per_render_quantum,
        &audio::callback,
        this);

      if (!open_status.success) {
        return;
      }

      auto start_status = audio_stream.start();
      if (!start_status.success) {
        return;
      }
    }
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to initialize PortAudio.", "AudioCore");
  }
}

AudioRenderer::Modification
AudioCore::make_add_renderable_modification(AudioRenderable* renderable) {
  AudioRenderer::Modification mod{};
  mod.renderable.value = renderable;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_recorder_modification(AudioRecorder* recorder) {
  AudioRenderer::Modification mod{};
  mod.recorder.value = recorder;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_transport_modification(Transport* transport) {
  AudioRenderer::Modification mod{};
  mod.transport.value = transport;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_scale_modification(AudioScale* scale) {
  AudioRenderer::Modification mod{};
  mod.scale.value = scale;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_audio_effect_modification(AudioEffect* effect) {
  AudioRenderer::Modification mod{};
  mod.audio_effect.value = effect;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_timeline_system_modification(TimelineSystem* sys) {
  AudioRenderer::Modification mod{};
  mod.timeline_system.value = sys;
  return mod;
}

AudioRenderer::Modification
AudioCore::make_add_note_clip_system_modification(NoteClipSystem* sys) {
  AudioRenderer::Modification mod{};
  mod.note_clip_system.value = sys;
  return mod;
}

void AudioCore::push_render_modification(const AudioRenderer::Modification& mod) {
  pending_renderer_modifications.push_back(mod);
}

void AudioCore::ui_update() {
  auto accessors = renderer.get_accessors();
  DynamicArray<int, 16> to_erase;

  for (int i = 0; i < int(pending_renderer_modifications.size()); i++) {
    auto& mod = pending_renderer_modifications[i];
    if (process_renderer_modification(accessors, mod)) {
      to_erase.push_back(i);
    } else {
      break;
    }
  }

  erase_set(pending_renderer_modifications, to_erase);
  update_accessors(accessors);

  renderer.get_audio_buffer_store()->ui_update();
}

GROVE_NAMESPACE_END