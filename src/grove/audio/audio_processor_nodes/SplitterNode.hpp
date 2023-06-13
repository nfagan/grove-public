#pragma once

#include "../audio_node.hpp"

namespace grove {

class SplitterNode : public AudioProcessorNode {
public:
  SplitterNode();
  ~SplitterNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;
private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;
};

}