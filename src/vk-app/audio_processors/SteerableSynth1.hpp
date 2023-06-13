#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"
#include "Reverb1.hpp"

namespace grove {

class AudioScale;
struct AudioParameterSystem;

class SteerableSynth1 : public AudioProcessorNode {
public:
  static constexpr int num_voices = 4;

public:
  SteerableSynth1(uint32_t node_id, const AudioParameterSystem* param_sys, const AudioScale* scale,
                  uint32_t pitch_sample_group_id);
  ~SteerableSynth1() override = default;

  GROVE_DECLARE_AUDIO_NODE_INTERFACE()
  GROVE_DECLARE_AUDIO_NODE_PARAMETER_DESCRIPTORS()
  uint32_t get_id() const override {
    return node_id;
  }

private:
  uint32_t node_id;
  const AudioParameterSystem* parameter_system;
  const AudioScale* scale;
  uint32_t pitch_sample_group_id;

  env::ADSRExp<float> envelopes[num_voices];
  audio::ExpInterpolated<float> pitch_bend{};
  audio::ExpInterpolated<float> amp_mod_gain{1.0f};
  osc::Sin oscillators[num_voices];
  uint8_t active_notes[num_voices]{};
  uint8_t pending_notes[num_voices]{};
  uint8_t num_pending_notes{};
  uint8_t latest_note_number{};

  AudioParameter<float, StaticLimits11<float>> pitch_bend_param{0.0f};
  AudioParameter<float, StaticLimits01<float>> reverb_mix{0.0f};
  AudioParameter<float, StaticLimits01<float>> noise_gain{0.0f};
  Reverb1 reverb;
  osc::WaveTable noise_osc;
  osc::Sin noise_amp_lfo{};
};

}