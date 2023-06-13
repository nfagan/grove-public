#include "AudioRenderer.hpp"
#include "grove/common/profile.hpp"
#include "AudioRenderable.hpp"
#include "AudioRecorder.hpp"
#include "AudioStream.hpp"
#include "AudioEffect.hpp"
#include "Transport.hpp"
#include "AudioScale.hpp"
#include "TimelineSystem.hpp"
#include "AudioEventSystem.hpp"
#include "AudioParameterSystem.hpp"
#include "AudioRenderBufferSystem.hpp"
#include "AudioNodeIsolator.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "QuantizedTriggeredNotes.hpp"
#include "ArpeggiatorSystem.hpp"
#include "PitchSamplingSystem.hpp"
#include "NoteClipStateMachineSystem.hpp"
#include "AudioScaleSystem.hpp"
#include "Metronome.hpp"
#include "grove/common/common.hpp"
#include "grove/common/config.hpp"
#include "grove/common/logging.hpp"
#include <algorithm>

#define LOG_IF_OUTPUT_BUFFER_UNDERFLOW (1)
#define LOG_IF_MAIN_THREAD_EVENT_BUFFER_OVERFLOW (0)

GROVE_NAMESPACE_BEGIN

namespace {

struct TryLock {
  explicit TryLock(std::atomic<bool>& in_use) noexcept : in_use(in_use) {
    acquired = in_use.compare_exchange_strong(acquired, true);
  }

  ~TryLock() {
    if (acquired) {
      in_use.store(false);
    }
  }

  std::atomic<bool>& in_use;
  bool acquired{false};
};

struct SpinLock {
  explicit SpinLock(std::atomic<bool>& in_use) noexcept : in_use(in_use) {
    bool acquired{false};

    while (!in_use.compare_exchange_strong(acquired, true)) {
      acquired = false;
    }
  }

  ~SpinLock() {
    in_use.store(false);
  }

  std::atomic<bool>& in_use;
};

template <typename T, typename U>
auto find_ptr(T&& ptrs, U value) {
  return std::find_if(ptrs.begin(), ptrs.end(), [value](const auto& up) {
    return up.get() == value;
  });
}

#if LOG_IF_MAIN_THREAD_EVENT_BUFFER_OVERFLOW
void warn_if_dropping_audio_events(int num_free, int num_pending) {
  if (num_free < num_pending) {
    GROVE_LOG_SEVERE_CAPTURE_META("Failed to output some events to main thread.", "AudioRenderer");
  }
}
#endif
void warn_if_failed_to_supply_sufficient_frames(int frames_supplied, int frames_expected) {
  if (frames_supplied < frames_expected) {
    GROVE_LOG_SEVERE_CAPTURE_META("Failed to supply sufficient number of frames.", "AudioRenderer");
  }
}
void warn_if_dropping_rendered_samples_and_events(int frames_output, int frames_expected) {
  if (frames_output < frames_expected) {
    GROVE_LOG_SEVERE_CAPTURE_META("Dropping some rendered sample and event frames.", "AudioRenderer");
  }
}

} //  anon

AudioRenderer::AudioRenderer() {
  std::fill(sample_buffer.begin(), sample_buffer.end(), 0.0f);
}

int AudioRenderer::render_quantum_samples() const {
  return num_output_channels * render_quantum_frames;
}

int AudioRenderer::num_samples_to_read() const {
  return sample_buffer.size();
}

int AudioRenderer::num_samples_free() const {
  return sample_buffer.num_free();
}

void AudioRenderer::output_samples(Sample* out, int frames_supplied, int frames_expected) noexcept {
  const int num_samples_read = frames_supplied * num_output_channels;
  const int num_samples_remaining = (frames_expected - frames_supplied) * num_output_channels;

  for (int i = 0; i < num_samples_read; i++) {
    *out++ = sample_buffer.read();
  }

  for (int i = 0; i < num_samples_remaining; i++) {
    *out++ = 0.0f;
  }
}

void AudioRenderer::discard_events(int num_frames_supplied) noexcept {
  for (int i = 0; i < num_frames_supplied; i++) {
    event_buffer.read();
  }

  dropped_some_events.store(true);
}

void AudioRenderer::write_events(int frames_supplied, int, double start_time) noexcept {
  const int main_thread_num_free = main_thread_event_buffer.num_free();
  const int num_events_write = std::min(frames_supplied, main_thread_num_free);
  const int num_discard = frames_supplied - num_events_write;

#if LOG_IF_MAIN_THREAD_EVENT_BUFFER_OVERFLOW
  warn_if_dropping_audio_events(main_thread_num_free, frames_supplied);
#endif

  const double sample_period = 1.0 / sample_rate;

  for (int i = 0; i < num_events_write; i++) {
    auto evts = event_buffer.read();
    const auto frame_start_time = start_time + double(i) * sample_period;

    for (auto& evt : evts) {
      evt.time = frame_start_time;
    }

    main_thread_event_buffer.write(evts);
  }

  for (int i = 0; i < num_discard; i++) {
    event_buffer.read();
  }

  if (num_discard > 0) {
    //  Mark that we had to discard some events.
    dropped_some_events.store(true);
  }
}

void AudioRenderer::output_events(int frames_supplied, int frames_expected,
                                  double start_time) noexcept {
  if (write_events_to_main_thread) {
    write_events(frames_supplied, frames_expected, start_time);
  } else {
    discard_events(frames_supplied);
  }
}

void AudioRenderer::output(Sample* out, int num_frames_expected, double start_time) noexcept {
  //  After changing certain audio stream parameters (like the number of output channels),
  //  it may be necessary to flush rendered samples and events generated using the prior stream's
  //  parameters. This is managed by the rendering thread, so we may have to drop some samples
  //  until the rendering thread responds to the new stream.
  TryLock lock{output_buffer_semaphore};
  if (!lock.acquired || num_output_channels == 0) {
    return;
  }

  const int num_sample_frames_supplied =
    std::min(sample_buffer.size()/num_output_channels, num_frames_expected);

  const int num_event_frames_supplied =
    std::min(event_buffer.size(), num_frames_expected);

  //  Output `num_frames_supplied` samples and events, to keep them in sync.
  const int num_frames_supplied =
    std::min(num_sample_frames_supplied, num_event_frames_supplied);

  output_samples(out, num_frames_supplied, num_frames_expected);
  output_events(num_frames_supplied, num_frames_expected, start_time);

#if LOG_IF_OUTPUT_BUFFER_UNDERFLOW
  warn_if_failed_to_supply_sufficient_frames(num_frames_supplied, num_frames_expected);
#endif
}

void AudioRenderer::enable_main_thread_events() {
  write_events_to_main_thread = true;
}

void AudioRenderer::disable_main_thread_events() {
  write_events_to_main_thread = false;
}

bool AudioRenderer::check_dropped_events() noexcept {
  bool dropped{true};
  return dropped_some_events.compare_exchange_strong(dropped, false);
}

bool AudioRenderer::check_output_buffer_underflow() noexcept {
  bool underflow{true};
  return output_buffer_underflow.compare_exchange_strong(underflow, false);
}

void AudioRenderer::mark_output_buffer_underflow() noexcept {
  output_buffer_underflow.store(true);
}

void AudioRenderer::set_cpu_usage_estimate(double val) noexcept {
  cpu_usage_estimate.store(val);
}

double AudioRenderer::get_cpu_usage_estimate() const noexcept {
  return cpu_usage_estimate.load();
}

void AudioRenderer::read_events(std::vector<AudioEvents>& events) {
  const int num_events = main_thread_event_buffer.size();
  for (int i = 0; i < num_events; i++) {
    events.push_back(main_thread_event_buffer.read());
  }
}

void AudioRenderer::append_rendered_samples_to_staging_buffer() noexcept {
  const int num_samples = render_quantum_samples();

  auto* dest_samples = staging_sample_buffer.get();
  auto* src_samples = per_renderable_sample_buffer.get();

  for (int i = 0; i < num_samples; i++) {
    dest_samples[i] += src_samples[i];
  }
}

void AudioRenderer::clear_renderable_samples() noexcept {
  auto* renderable_samples = per_renderable_sample_buffer.get();
  std::fill(renderable_samples, renderable_samples + render_quantum_samples(), Sample(0));
}

void AudioRenderer::clear_staging_buffers() noexcept {
  auto* staging_samples = staging_sample_buffer.get();
  auto* staging_events = staging_events_buffer.get();

  std::fill(staging_samples, staging_samples + render_quantum_samples(), Sample(0));

  for (int i = 0; i < render_quantum_frames; i++) {
    staging_events[i].clear();
  }
}

void AudioRenderer::maybe_apply_new_stream_info(const AudioStreamInfo& stream_info) noexcept {
  if (num_output_channels != stream_info.num_output_channels ||
      sample_rate != stream_info.sample_rate ||
      render_quantum_frames != stream_info.frames_per_render_quantum) {
    //
    SpinLock lock{output_buffer_semaphore};

    num_output_channels = stream_info.num_output_channels;
    sample_rate = stream_info.sample_rate;
    render_quantum_frames = stream_info.frames_per_render_quantum;

    auto new_num_samples = render_quantum_samples();
    if (new_num_samples > 0) {
      staging_sample_buffer = std::make_unique<Sample[]>(new_num_samples);
      per_renderable_sample_buffer = std::make_unique<Sample[]>(new_num_samples);

    } else {
      staging_sample_buffer = nullptr;
      per_renderable_sample_buffer = nullptr;
    }

    if (render_quantum_frames > 0) {
      //  render_quantum_samples() can be 0 if there are no output channels, but we may still
      //  have events.
      staging_events_buffer = std::make_unique<AudioEvents[]>(render_quantum_frames);
    } else {
      staging_events_buffer = nullptr;
    }

    sample_buffer.clear();
    event_buffer.clear();
  }
}

void AudioRenderer::push_rendered_samples_to_output_buffer() noexcept {
  auto* staging_samples = staging_sample_buffer.get();
  auto* staging_events = staging_events_buffer.get();

  const int num_sample_frames =
    std::min(num_samples_free() / num_output_channels, render_quantum_frames);
  const int num_event_frames =
    std::min(event_buffer.num_free(), render_quantum_frames);

  const int num_output_frames = std::min(num_sample_frames, num_event_frames);
  const int num_samples = num_output_frames * num_output_channels;

#if LOG_IF_OUTPUT_BUFFER_UNDERFLOW
  warn_if_dropping_rendered_samples_and_events(num_output_frames, render_quantum_frames);
#endif

  sample_buffer.write_range_copy(staging_samples, staging_samples + num_samples);
  event_buffer.write_range_move(staging_events, staging_events + num_output_frames);
}

void AudioRenderer::render(double output_time) noexcept {
//  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("AudioRenderer/render");

  auto* staging_samples = staging_sample_buffer.get();
  auto* renderable_samples = per_renderable_sample_buffer.get();
  auto* staging_events = staging_events_buffer.get();

  AudioRenderInfo info{sample_rate, render_quantum_frames,
                       num_output_channels, render_frame_index};

  const auto& read_renderables = renderable_accessor.maybe_swap_and_read();
  const auto& read_transports = transport_accessor.maybe_swap_and_read();
  const auto& read_scales = scale_accessor.maybe_swap_and_read();
  const auto& read_recorders = recorder_accessor.maybe_swap_and_read();
  const auto& read_effects = effect_accessor.maybe_swap_and_read();
  const auto& read_timeline_systems = timeline_systems_accessor.maybe_swap_and_read();
  const auto& read_note_clip_systems = note_clip_systems_accessor.maybe_swap_and_read();

#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
  audio_event_system::render_begin_process();
#endif
  audio_buffer_system::render_begin_process();

  audio_buffer_store->render_update();

  for (auto* transport : read_transports) {
    transport->begin_render(info);
  }

  for (auto* scale : read_scales) {
    scale->begin_render();
  }

  scale_system::render_begin_process(scale_system::get_global_audio_scale_system(), info);
  pss::render_begin_process(pss::get_global_pitch_sampling_system(), info);

  for (auto* recorder : read_recorders) {
    recorder->begin_render(info);
  }

  for (auto* note_clip_system : read_note_clip_systems) {
    begin_render(note_clip_system);
  }

  AudioNodeIsolator* node_isolator = ni::get_global_audio_node_isolator();
  ni::begin_render(node_isolator, info);

  //  Update audio parameter values.
  param_system::render_begin_process(param_system::get_global_audio_parameter_system(), info);

  auto* midi_message_stream_sys = midi::get_global_midi_message_stream_system();
  midi::render_begin_process(midi_message_stream_sys, info);

  auto* triggered_notes = notes::get_global_triggered_notes();
  auto triggered_note_changes = notes::render_begin_process(triggered_notes);
  notes::render_push_messages_to_streams(midi_message_stream_sys, triggered_note_changes);

  for (auto* sys : read_timeline_systems) {
    process(sys, triggered_notes, info);
  }

  qtn::begin_process(qtn::get_global_quantized_triggered_notes(), midi_message_stream_sys, info);
  arp::render_begin_process(arp::get_global_arpeggiator_system(), info);
  ncsm::render_begin_process(ncsm::get_global_note_clip_state_machine(), info);

  //  has to come before rendering below
  midi::render_write_streams(midi_message_stream_sys);

  for (auto* renderable : read_renderables) {
    clear_renderable_samples();
    renderable->render(*this, renderable_samples, staging_events, info);
    ni::process(node_isolator, renderable, renderable_samples, info);
    append_rendered_samples_to_staging_buffer();
  }

  {
    clear_renderable_samples();
    metronome::render_process(metronome::get_global_metronome(), renderable_samples, info);
    append_rendered_samples_to_staging_buffer();
  }

  notes::render_end_process(notes::get_global_triggered_notes());

  midi::render_end_process(midi_message_stream_sys);

  for (auto* recorder : read_recorders) {
    recorder->end_render(info);
  }

  for (auto* transport : read_transports) {
    transport->end_render(info);
  }

  for (auto* effect : read_effects) {
    //  @TODO: Enable parameter changes for global audio effects.
    effect->process(staging_samples, staging_events, AudioParameterChangeView{}, info);
  }

  ni::end_render(node_isolator);

  audio_buffer_system::render_end_process();

#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
  audio_event_system::render_end_process(output_time, sample_rate);
#endif

  push_rendered_samples_to_output_buffer();
  clear_staging_buffers();

  render_frame_index += render_quantum_frames;
}

GROVE_NAMESPACE_END
