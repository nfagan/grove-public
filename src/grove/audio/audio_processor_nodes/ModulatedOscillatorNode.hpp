#pragma once

#include "../audio_node.hpp"
#include "../oscillator.hpp"

namespace grove {

class AudioScale;

class ModulatedOscillatorNode : public AudioProcessorNode {
public:
  static constexpr int num_output_ports = 2;
public:
  explicit ModulatedOscillatorNode(const AudioScale* scale);
  ~ModulatedOscillatorNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;
private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;
  osc::WaveTable oscillator;
  const AudioScale* scale;

  double freq_mod_depth = 0.005;
  double gain_mod_depth = 1.0;
  uint8_t current_note_number;
  double center_frequency;
};

}