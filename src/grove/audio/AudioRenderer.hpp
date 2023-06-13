#pragma once

#include "types.hpp"
#include "grove/common/RingBuffer.hpp"
#include "audio_events.hpp"
#include "AudioBufferStore.hpp"
#include "DoubleBuffer.hpp"
#include <array>
#include <vector>
#include <memory>
#include <atomic>

namespace grove {

class AudioRenderable;
class Transport;
class AudioScale;
class AudioEffect;
struct AudioParameterSystem;
class AudioRecorder;
struct AudioStreamInfo;
struct TimelineSystem;
struct NoteClipSystem;

class AudioRenderer {
private:
  static constexpr int sample_buffer_size = 8192;
  static constexpr int event_buffer_size = 4096;

public:
  template <typename T>
  using DoubleBuffer = audio::DoubleBuffer<T>;

  template <typename T>
  using DoubleBufferAccessor = audio::DoubleBufferAccessor<T>;

  template <typename T>
  using Vec = std::vector<T>;

  using Renderables = DoubleBuffer<Vec<AudioRenderable*>>;
  using Transports = DoubleBuffer<Vec<Transport*>>;
  using Scales = DoubleBuffer<Vec<AudioScale*>>;
  using Recorders = DoubleBuffer<Vec<AudioRecorder*>>;
  using Effects = DoubleBuffer<Vec<AudioEffect*>>;
  using TimelineSystems = DoubleBuffer<DynamicArray<TimelineSystem*, 2>>;
  using ClipSystems = DoubleBuffer<DynamicArray<NoteClipSystem*, 2>>;

  using AccessRenderables = DoubleBufferAccessor<Vec<AudioRenderable*>>;
  using AccessTransports = DoubleBufferAccessor<Vec<Transport*>>;
  using AccessScales = DoubleBufferAccessor<Vec<AudioScale*>>;
  using AccessRecorders = DoubleBufferAccessor<Vec<AudioRecorder*>>;
  using AccessEffects = DoubleBufferAccessor<Vec<AudioEffect*>>;
  using AccessTimelineSystems = DoubleBufferAccessor<DynamicArray<TimelineSystem*, 2>>;
  using AccessClipSystems = DoubleBufferAccessor<DynamicArray<NoteClipSystem*, 2>>;

  template <typename T>
  struct AddOrRemove {
    bool has_value() const {
      return value != nullptr;
    }

    T* value{};
    bool remove{};
  };

  struct Modification {
    AddOrRemove<AudioRenderable> renderable{};
    AddOrRemove<Transport> transport{};
    AddOrRemove<AudioScale> scale{};
    AddOrRemove<AudioEffect> audio_effect{};
    AddOrRemove<AudioRecorder> recorder{};
    AddOrRemove<TimelineSystem> timeline_system{};
    AddOrRemove<NoteClipSystem> note_clip_system{};
  };

  struct Accessors {
    AccessRenderables* renderables{};
    AccessTransports* transports{};
    AccessScales* scales{};
    AccessRecorders* recorders{};
    AccessEffects* effects{};
    AccessTimelineSystems* timeline_systems{};
    AccessClipSystems* note_clip_systems{};
  };

public:
  AudioRenderer();

  void output(Sample* out, int num_frames, double start_time) noexcept;
  void render(double output_time) noexcept;
  void maybe_apply_new_stream_info(const AudioStreamInfo& stream_info) noexcept;

  void enable_main_thread_events();
  void disable_main_thread_events();
  void read_events(std::vector<AudioEvents>& events);

  bool check_dropped_events() noexcept;
  bool check_output_buffer_underflow() noexcept;
  void mark_output_buffer_underflow() noexcept;

  double get_cpu_usage_estimate() const noexcept;
  void set_cpu_usage_estimate(double val) noexcept;

  int render_quantum_samples() const;
  int num_samples_to_read() const;

  AudioBufferStore* get_audio_buffer_store() {
    return audio_buffer_store.get();
  }

  const AudioBufferStore* get_audio_buffer_store() const {
    return audio_buffer_store.get();
  }

  Accessors get_accessors() noexcept {
    return {
      &renderable_accessor,
      &transport_accessor,
      &scale_accessor,
      &recorder_accessor,
      &effect_accessor,
      &timeline_systems_accessor,
      &note_clip_systems_accessor
    };
  }

private:
  int num_samples_free() const;

  void output_samples(Sample* out, int frames_supplied, int frames_expected) noexcept;
  void output_events(int frames_supplied, int frames_expected, double start_time) noexcept;

  void write_events(int frames_supplied, int frames_expected, double start_time) noexcept;
  void discard_events(int frames_supplied) noexcept;

  void clear_renderable_samples() noexcept;
  void clear_staging_buffers() noexcept;
  void append_rendered_samples_to_staging_buffer() noexcept;
  void push_rendered_samples_to_output_buffer() noexcept;

private:
  using EventBufferStorage =
    RingBufferHeapStorage<AudioEvents, event_buffer_size>;

private:
  Transports transports;
  AccessTransports transport_accessor{transports};

  Scales scales;
  AccessScales scale_accessor{scales};

  Renderables renderables;
  AccessRenderables renderable_accessor{renderables};

  Recorders recorders;
  AccessRecorders recorder_accessor{recorders};

  Effects audio_effects;
  AccessEffects effect_accessor{audio_effects};

  std::unique_ptr<AudioBufferStore> audio_buffer_store{std::make_unique<AudioBufferStore>()};

  TimelineSystems timeline_systems;
  AccessTimelineSystems timeline_systems_accessor{timeline_systems};

  ClipSystems note_clip_systems;
  AccessClipSystems note_clip_systems_accessor{note_clip_systems};

  double sample_rate{default_sample_rate()};
  int num_output_channels{0};
  int render_quantum_frames{};

  RingBuffer<Sample, sample_buffer_size> sample_buffer;
  RingBuffer<AudioEvents, event_buffer_size, EventBufferStorage> event_buffer;
  RingBuffer<AudioEvents, event_buffer_size, EventBufferStorage> main_thread_event_buffer;

  std::unique_ptr<Sample[]> staging_sample_buffer;
  std::unique_ptr<Sample[]> per_renderable_sample_buffer;
  std::unique_ptr<AudioEvents[]> staging_events_buffer;

  uint64_t render_frame_index{};

  std::atomic<bool> write_events_to_main_thread{false};
  std::atomic<bool> output_buffer_semaphore{false};
  std::atomic<bool> dropped_some_events{false};
  std::atomic<bool> output_buffer_underflow{false};
  std::atomic<double> cpu_usage_estimate{0};
};

}