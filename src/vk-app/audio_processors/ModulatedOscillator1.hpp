#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

class AudioScale;

class ModulatedOscillator1 : public AudioProcessorNode {
public:
  explicit ModulatedOscillator1(const AudioScale* scale);
  ~ModulatedOscillator1() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  struct OscParams {
    float amplitude;
    float frequency_modulation;
  };

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  const AudioScale* scale;
  osc::WaveTable oscillator;
  double center_frequency{};
  float current_cv{};
  OscParams osc_params{};
};

}