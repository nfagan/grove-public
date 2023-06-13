#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include "grove/audio/oscillator.hpp"

namespace grove {

class ModulatedEnvelope : public AudioProcessorNode {
public:
  ModulatedEnvelope();
  ~ModulatedEnvelope() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  env::ADSRExp<float> envelope;
  osc::Sin lfo;

  Envelope::Params reference_params;
  Envelope::Params current_params;
};

}