#pragma once

#include "grove/audio/audio_buffer.hpp"
#include <functional>

namespace grove::audio {

struct PendingAudioBufferAvailable {
  AudioBufferDescriptor descriptor;
  std::unique_ptr<unsigned char[]> data;
  std::function<void(AudioBufferHandle)> callback;
};

}