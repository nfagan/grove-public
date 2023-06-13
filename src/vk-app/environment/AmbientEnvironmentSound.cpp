#include "AmbientEnvironmentSound.hpp"
#include "../audio_core/AudioBuffers.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/audio/io.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "AmbientEnvironmentSound";
}

} //  anon

AmbientEnvironmentSound::InitResult AmbientEnvironmentSound::initialize() {
  InitResult result{};

  {
    auto idle_file = AudioBuffers::audio_buffer_full_path("wind2.wav");
    auto idle_load_result = io::read_wav_as_float(idle_file.c_str());
    if (idle_load_result.success) {
      audio::PendingAudioBufferAvailable pend{};
      pend.descriptor = std::move(idle_load_result.descriptor);
      pend.data = std::move(idle_load_result.data);
      pend.callback = [this](AudioBufferHandle handle) {
        idle_buffer_handle = handle;
      };
      result.pending_buffers.push_back(std::move(pend));
    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to load idle sound.", logging_id());
    }
  }

  {
    const bool normalize = true;
    const bool max_normalize = true;

    auto rain_file = AudioBuffers::audio_buffer_full_path("light-rain.wav");
    auto rain_load_result = io::read_wav_as_float(rain_file.c_str(), normalize, max_normalize);
    if (rain_load_result.success) {
      audio::PendingAudioBufferAvailable pend{};
      pend.descriptor = std::move(rain_load_result.descriptor);
      pend.data = std::move(rain_load_result.data);
      pend.callback = [this](AudioBufferHandle handle) {
        rain_buffer_handle = handle;
      };
      result.pending_buffers.push_back(std::move(pend));
    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to load rain sound.", logging_id());
    }
  }

  return result;
}

AmbientEnvironmentSound::UpdateResult
AmbientEnvironmentSound::update(const UpdateInfo&) {
  UpdateResult result{};

  if (idle_buffer_handle && !idle_sound.is_valid()) {
    TriggeredBufferPlayParams play_params{};
    play_params.gain = 20.0f;
    play_params.loop_type = TriggeredBufferLoopType::Forwards;

    PendingPlay pend{};
    pend.handle = idle_buffer_handle.value();
    pend.params = play_params;
    pend.assign_instance = &idle_sound;
    result.to_play.push_back(pend);
  }

  if (rain_buffer_handle && !rain_sound.is_valid()) {
    TriggeredBufferPlayParams play_params{};
    play_params.gain = 0.0f;
    play_params.loop_type = TriggeredBufferLoopType::Forwards;

    PendingPlay pend{};
    pend.handle = rain_buffer_handle.value();
    pend.params = play_params;
    pend.assign_instance = &rain_sound;
    result.to_play.push_back(pend);
  }

  if (idle_gain && idle_sound.is_valid()) {
    TriggeredBufferRenderer::PendingModification mod{};
    mod.handle = idle_sound.get_handle();
    mod.gain = idle_gain.value();
    result.triggered_modifications.push_back(mod);
    idle_gain = NullOpt{};
  }

  if (rain_gain && rain_sound.is_valid()) {
    TriggeredBufferRenderer::PendingModification mod{};
    mod.handle = rain_sound.get_handle();
    mod.gain = rain_gain.value();
    result.triggered_modifications.push_back(mod);
    rain_gain = NullOpt{};
  }

  return result;
}

void AmbientEnvironmentSound::set_rain_gain_frac(float gain01) {
  rain_gain = lerp(gain01, 0.0f, 0.25f);
}

GROVE_NAMESPACE_END
