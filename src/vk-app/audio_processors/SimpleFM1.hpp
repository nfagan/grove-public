#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/envelope.hpp"

namespace grove {

struct AudioParameterSystem;
class AudioScale;

class SimpleFM1 : public AudioProcessorNode {
public:
  SimpleFM1(uint32_t node_id, const AudioParameterSystem* param_sys, const AudioScale* scale);
  ~SimpleFM1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;
  const AudioParameterSystem* param_sys;
  const AudioScale* scale;

  double carrier_phase{};
  double modulator_phase{};

  AudioParameter<float, StaticLimits01<float>> fd_freq{0.0f};
  AudioParameter<float, StaticLimits01<float>> fm_freq{0.0f};
  AudioParameter<float, StaticLimits01<float>> fm_depth{0.0f};
  AudioParameter<float, StaticLimits01<float>> detune{0.5f};

  uint8_t note_num{midi_note_number_a4()};
  audio::ExpInterpolated<float> carrier_frequency{float(frequency_a4())};
};

}