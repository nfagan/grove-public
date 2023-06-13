#pragma once

#include "grove/audio/MIDIInstrument.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/audio/envelope.hpp"

namespace grove {

class RandomizedInstrument : public MIDIInstrument {
public:
  static constexpr int num_voices = 8;
  static constexpr float default_gain = -7.0f;
  GROVE_DECLARE_CONSTEXPR_FLOAT_LIMITS(GainLimits, -30.0f, 0.0f);

public:
  explicit RandomizedInstrument(AudioParameterID node_id);
  ~RandomizedInstrument() override = default;

  void process(const InputData& in,
               const OutputData& out,
               const AudioRenderInfo& info) override;

  AudioParameterDescriptors parameter_descriptors() const override;
  AudioParameterID parameter_parent_id() const override;

  const char* name() const override {
    return "RandomizedInstrument";
  }

  void reset() override {}

private:
  void randomize_frequencies(int key, int off);
  void apply_new_waveform_type(int new_type);

private:
  AudioParameterID node_id;

  DynamicArray<osc::WaveTable, num_voices> oscillators;
  DynamicArray<env::ADSR, num_voices> envelopes;

  osc::WaveTable amp_mod_lfo{44.1e3, 8.0};

  AudioParameter<int, StaticIntLimits<0, 2>> waveform_type{0};
  AudioParameter<int, StaticIntLimits<0, 2>> note_key{0};
  AudioParameter<int, StaticIntLimits<0, 12>> note_offset{0};
  AudioParameter<float, StaticLimits01<float>> amp_mod_lfo_depth{0.0f};
  AudioParameter<float, GainLimits> gain{default_gain};
  DynamicArray<AudioParameter<float, GainLimits>, num_voices> envelope_representations;
};

}