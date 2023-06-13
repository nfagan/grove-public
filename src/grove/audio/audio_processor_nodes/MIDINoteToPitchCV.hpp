#pragma once

#include "grove/audio/audio_node.hpp"

namespace grove {

class MIDINoteToPitchCV : public AudioProcessorNode {
public:
  MIDINoteToPitchCV(double min_semitone,
                    double max_semitone,
                    double min_cv = -1.0,
                    double max_cv = 1.0);
  ~MIDINoteToPitchCV() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  InputAudioPorts input_ports;
  OutputAudioPorts output_ports;

  double min_semitone{};
  double max_semitone{};
  double min_cv{};
  double max_cv{};

  float cv_value{};
};

}