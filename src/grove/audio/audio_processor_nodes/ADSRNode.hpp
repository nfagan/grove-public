#pragma once

#include "../audio_node.hpp"
#include "../envelope.hpp"

namespace grove {

class ADSRNode : public AudioProcessorNode {
public:
  static constexpr int num_output_channels = 1;
public:
  ADSRNode();
  ~ADSRNode() override = default;

  InputAudioPorts inputs() const override;
  OutputAudioPorts outputs() const override;

  void process(const AudioProcessData& in,
               const AudioProcessData& out,
               AudioEvents* events,
               const AudioRenderInfo& info) override;

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  env::ADSR envelope;
  int num_notes_on;
  bool pending_note_off;
};

}