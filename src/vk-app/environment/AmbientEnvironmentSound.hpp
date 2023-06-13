#pragma once

#include "../audio_core/common.hpp"
#include "grove/audio/TriggeredBufferRenderer.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

class AmbientEnvironmentSound {
public:
  struct InitResult {
    DynamicArray<audio::PendingAudioBufferAvailable, 2> pending_buffers;
  };

  struct UpdateInfo {
    //
  };

  struct PendingPlay {
    AudioBufferHandle handle;
    TriggeredBufferPlayParams params;
    UITriggeredBufferInstance* assign_instance{};
  };

  struct UpdateResult {
    DynamicArray<PendingPlay, 2> to_play;
    DynamicArray<TriggeredBufferRenderer::PendingModification, 2> triggered_modifications;
  };

public:
  InitResult initialize();
  UpdateResult update(const UpdateInfo& update_info);
  void set_rain_gain_frac(float gain01);

private:
  Optional<AudioBufferHandle> idle_buffer_handle;
  Optional<AudioBufferHandle> rain_buffer_handle;
  UITriggeredBufferInstance idle_sound;
  UITriggeredBufferInstance rain_sound;

  Optional<float> idle_gain;
  Optional<float> rain_gain;
};

}