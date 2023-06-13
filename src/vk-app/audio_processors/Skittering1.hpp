#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/audio_buffer.hpp"

namespace grove {

class AudioBufferStore;
class Transport;
struct PitchSamplingSystem;
struct AudioParameterSystem;
class AudioScale;

class Skittering1 : public AudioProcessorNode {
public:
  static constexpr int num_voices = 8;

  struct Voice {
    int st_phase;
    double fi;
    bool arp;
    float pending_midi_st;
    float curr_midi_st;
  };

  struct Params {
    static constexpr int num_params = 3;
    AudioParameter<int, StaticIntLimits<0, 1>> prefer_midi_input{0};
    AudioParameter<float, StaticLimits01<float>> arp_mix{0.0f};
    AudioParameter<float, StaticLimits01<float>> overall_gain{1.0f};
  };

public:
  Skittering1(
    uint32_t node_id, const AudioBufferStore* buff_store,
    const Transport* transport, const AudioScale* scale,
    const AudioParameterSystem* param_sys, uint32_t pitch_sample_group, AudioBufferHandle buffer);
  ~Skittering1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;
  Voice voices[num_voices]{};
  float global_semitone_offset{};

  const AudioBufferStore* buffer_store;
  const Transport* transport;
  const AudioScale* scale;
  const AudioParameterSystem* param_sys;
  uint32_t pitch_sample_group;
  AudioBufferHandle buffer_handle;

  Params params{};
};

}