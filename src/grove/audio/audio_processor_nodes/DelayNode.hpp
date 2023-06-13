#pragma once

#include "../audio_node.hpp"

namespace grove {

class DelayNode : public AudioProcessorNode {
public:
  explicit DelayNode(double delay_time = 0.1, double mix = 0.5);
  ~DelayNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;
private:
  int read_ptr(int delay_frames) const;
  void make_buffer();

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  double delay_time;
  double mix;
  double sample_rate{default_sample_rate()};

  std::unique_ptr<Sample[]> buffer;
  int wp;
};

}