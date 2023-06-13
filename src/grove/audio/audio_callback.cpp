#include "audio_callback.hpp"
#include "AudioCore.hpp"
#include "grove/common/common.hpp"
#include "audio_config.hpp"
#include <portaudio.h>
#include <chrono>

GROVE_NAMESPACE_BEGIN

namespace {

#if GROVE_RENDER_AUDIO_IN_CALLBACK

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;

inline auto make_cpu_usage_estimator(unsigned long frames_per_buffer, double sample_rate) {
  const auto t0 = Clock::now();
  return [frames_per_buffer, sample_rate, t0]() -> double {
    auto dur = Duration(Clock::now() - t0).count();
    auto max_possible = double(frames_per_buffer) / sample_rate;
    return dur / max_possible;
  };
}

#endif

} //  anon

int audio::callback(const void* input_buffer, void* output_buffer,
                    unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info,
                    unsigned long status, void* really_core) noexcept {

  (void) status;
  (void) input_buffer;

#if GROVE_RENDER_AUDIO_IN_CALLBACK
  auto* out = static_cast<Sample*>(output_buffer);
  auto* core = static_cast<AudioCore*>(really_core);
  auto& renderer = core->renderer;
  auto* stream_info = core->audio_stream.get_stream_info();
  assert(stream_info->frames_per_buffer == stream_info->frames_per_render_quantum);

  auto cpu_usage_estimator =
    make_cpu_usage_estimator(frames_per_buffer, stream_info->sample_rate);

  renderer.maybe_apply_new_stream_info(*stream_info);
  renderer.render(time_info->outputBufferDacTime);
  renderer.output(out, frames_per_buffer, time_info->outputBufferDacTime);

  if (status & paOutputUnderflow) {
    renderer.mark_output_buffer_underflow();
  }

  renderer.set_cpu_usage_estimate(cpu_usage_estimator());

#else
  auto* out = static_cast<Sample*>(output_buffer);
  auto* core = static_cast<AudioCore*>(really_core);
  auto& renderer = core->renderer;

  renderer.output(out, frames_per_buffer, time_info->outputBufferDacTime);
#endif

  return 0;
}

GROVE_NAMESPACE_END
