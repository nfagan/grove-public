#pragma once

#include "AudioRenderer.hpp"
#include "AudioThread.hpp"
#include "AudioStream.hpp"
#include "AudioRecorder.hpp"
#include "audio_config.hpp"

namespace grove {

struct AudioDeviceInfo;

class AudioCore {
public:
  struct FrameInfo {
    int frames_per_buffer{};
    int frames_per_render_quantum{};
  };

public:
  AudioCore();
  ~AudioCore();

  void initialize(bool start_default_stream, int desired_frames = -1);
  void terminate();
  void ui_update();
  void toggle_stream_started();
  bool change_stream(const AudioDeviceInfo& input_device,
                     const AudioDeviceInfo& output_device,
                     const FrameInfo& frame_info);
  bool change_stream(const AudioDeviceInfo& target_device, const FrameInfo& frame_info);
  bool change_stream(const FrameInfo& frame_info);
  bool change_stream(const AudioDeviceInfo& target_device);

  FrameInfo get_frame_info() const;
  void push_render_modification(const AudioRenderer::Modification& mod);

  static AudioRenderer::Modification make_add_renderable_modification(AudioRenderable* renderable);
  static AudioRenderer::Modification make_add_transport_modification(Transport* transport);
  static AudioRenderer::Modification make_add_scale_modification(AudioScale* scale);
  static AudioRenderer::Modification make_add_audio_effect_modification(AudioEffect* effect);
  static AudioRenderer::Modification make_add_recorder_modification(AudioRecorder* recorder);
  static AudioRenderer::Modification make_add_timeline_system_modification(TimelineSystem* sys);
  static AudioRenderer::Modification make_add_note_clip_system_modification(NoteClipSystem* sys);

private:
  int num_input_channels{0};
  int num_output_channels{2};
  double sample_rate{44.1e3};

#if GROVE_RENDER_AUDIO_IN_CALLBACK
  int frames_per_buffer{128};
  int frames_per_render_quantum = frames_per_buffer;
#else
  int frames_per_buffer{256};
  int frames_per_render_quantum{512};
#endif

  audio::SampleFormat sample_format{audio::SampleFormat::Float};

public:
  AudioRenderer renderer;
  AudioStream audio_stream;
  AudioThread audio_thread{&audio_stream, &renderer};
  AudioRecorder audio_recorder;

private:
  std::vector<AudioRenderer::Modification> pending_renderer_modifications;
};

}