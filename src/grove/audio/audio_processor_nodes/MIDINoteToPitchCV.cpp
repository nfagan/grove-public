#include "MIDINoteToPitchCV.hpp"
#include "../tuning.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

MIDINoteToPitchCV::MIDINoteToPitchCV(double min_semitone,
                                     double max_semitone,
                                     double min_cv,
                                     double max_cv) :
  min_semitone{min_semitone},
  max_semitone{max_semitone},
  min_cv{min_cv},
  max_cv{max_cv},
  cv_value{float(min_cv)} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
}

InputAudioPorts MIDINoteToPitchCV::inputs() const {
  return input_ports;
}

OutputAudioPorts MIDINoteToPitchCV::outputs() const {
  return output_ports;
}

void MIDINoteToPitchCV::process(const AudioProcessData& in,
                                const AudioProcessData& out,
                                AudioEvents*,
                                const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  const auto& message_descriptor = in.descriptors[0];
  const auto& cv_descriptor = out.descriptors[0];

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    message_descriptor.read(in.buffer.data, i, &message);

    if (message.is_note_on()) {
      auto st = note_number_to_semitone(message.note_number());
      st = clamp(st, min_semitone, max_semitone);
      const double st_frac = (st - min_semitone) / (max_semitone - min_semitone);
      cv_value = float(st_frac * (max_cv - min_cv) + min_cv);
    }

    cv_descriptor.write(out.buffer.data, i, &cv_value);
  }
}

GROVE_NAMESPACE_END
