#pragma once

#include <thread>
#include <atomic>
#include <mutex>

namespace grove {

class AudioStream;
class AudioRenderer;

class AudioThread {
public:
  static constexpr int num_ms_sleep = 5;
  static constexpr int num_render_quanta_per_update = 2;

public:
  AudioThread(AudioStream* stream, AudioRenderer* renderer);

  void start();
  void stop();
  bool proceed() const;

  void finished();
  std::thread::id thread_id() const;

private:
  mutable std::mutex mutex;

  AudioStream* stream;
  AudioRenderer* renderer;

  std::thread thread;
  std::atomic<bool> can_proceed{true};
  std::atomic<bool> thread_active{false};
};

}