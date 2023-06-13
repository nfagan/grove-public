#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/envelope.hpp"
#include <array>

namespace grove {

class Transport;

class EnvelopeSequencer : public AudioProcessorNode {
public:
  explicit EnvelopeSequencer(const Transport* transport);
  ~EnvelopeSequencer() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

private:
  static constexpr int num_steps = 8;

  OutputAudioPorts output_ports;
  InputAudioPorts input_ports;
  const Transport* transport;

//  env::ADSRExp<float> envelope;
  ScoreCursor cursor{};

  audio::Quantization quantization{audio::Quantization::Quarter};
  int step_index{0};
  std::array<env::ADSRExp<float>, num_steps> envelopes{};
};

}