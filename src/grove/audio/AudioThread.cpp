#include "AudioThread.hpp"
#include "AudioStream.hpp"
#include "AudioRenderer.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace {

void process(AudioThread* audio_thread, AudioStream* stream, AudioRenderer* renderer) {
  using millis = std::chrono::milliseconds;
  constexpr auto num_quanta = AudioThread::num_render_quanta_per_update;

  while (audio_thread->proceed()) {
    if (stream->is_stream_started()) {
      const auto num_read = renderer->num_samples_to_read();
      renderer->maybe_apply_new_stream_info(*stream->get_stream_info());

      if (num_read < renderer->render_quantum_samples() * num_quanta) {
        for (int i = 0; i < num_quanta; i++) {
          renderer->render(-1.0);
        }
      }
    }

    std::this_thread::sleep_for(millis(AudioThread::num_ms_sleep));
  }

  audio_thread->finished();
}

}

/*
 * AudioThread
 */

AudioThread::AudioThread(AudioStream* stream, AudioRenderer* renderer) :
  stream(stream),
  renderer(renderer) {
  //
}

bool AudioThread::proceed() const {
  return can_proceed;
}

void AudioThread::start() {
  std::lock_guard<std::mutex> lock(mutex);
  assert(!thread_active);

  thread_active = true;
  thread = std::thread([this]() {
    process(this, this->stream, this->renderer);
  });
}

void AudioThread::stop() {
  //  @Note: Don't put a lock here; can deadlock with call to finished()
  can_proceed = false;

  if (thread.joinable()) {
    thread.join();

  } else {
    thread_active = false;
    can_proceed = true;
  }
}

void AudioThread::finished() {
  std::lock_guard<std::mutex> lock(mutex);
  thread_active = false;
  can_proceed = true;
}

std::thread::id AudioThread::thread_id() const {
  return thread.get_id();
}

GROVE_NAMESPACE_END
