#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_buffer.hpp"
#include "grove/audio/delay.hpp"

namespace grove {

class AudioBufferStore;
class AudioScale;
struct AudioParameterSystem;
class Transport;

class MultiComponentSampler : public AudioProcessorNode {
public:
  static constexpr int max_num_voices = 8;

  struct GranulatorVoice {
    double fi;
    int yi;
    int gi;
    int sample_size;
    int fade_in_sample_size;
    double st_offset;
    double st_noise;
    double sin_phase;
    bool use_sin;
    bool masked_out;
    uint8_t quantized_state;
  };

  struct Voice {
    GranulatorVoice granulator;
  };

  struct Params {
    static constexpr int num_params = 6;
    AudioParameter<float, StaticLimits01<float>> granule_dur{0.0f};
    AudioParameter<float, StaticLimits01<float>> voice_delay_mix{0.0f};
    AudioParameter<float, StaticLimits01<float>> p_sin{0.0f};
    AudioParameter<float, StaticLimits01<float>> p_masked_out{0.0f};
    AudioParameter<float, StaticLimits01<float>> p_quantized_granule_dur{0.0f};
    AudioParameter<int, StaticIntLimits<0, 4>> note_set_index{0};
  };

public:
  MultiComponentSampler(
    uint32_t node_id, const AudioBufferStore* buffer_store,
    const AudioBufferHandle* buff_handles, int num_handles, const AudioScale* scale,
    const Transport* transport, const AudioParameterSystem* param_sys, uint32_t pitch_sample_group);
  ~MultiComponentSampler() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;

  const AudioBufferStore* buffer_store;
  AudioBufferHandle buff_handles[max_num_voices]{};
  int num_buff_handles{};

  const AudioScale* scale;
  const Transport* transport;
  const AudioParameterSystem* param_sys;
  uint32_t pitch_sample_group;

  double kb_semitone{semitone_a4()};

  Voice voices[max_num_voices]{};
  double global_possible_st_offsets[16]{};
  int num_global_st_offsets{};

  double global_grain_dur{};
  double global_grain_dur_noise_prop{0.2};
  audio::Quantization global_quantized_grain_dur{};

  audio::InterpolatedDelayLine<float> voice_delay0;
  audio::InterpolatedDelayLine<float> voice_delay1;

  Params params{};
};

}