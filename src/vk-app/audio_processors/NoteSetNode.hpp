#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

class NoteSetNode : public AudioProcessorNode {
public:
  NoteSetNode();
  ~NoteSetNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

  MIDINote render_get_randomized_note() const;

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  int key{};
};

}