#include "NoteSetNode.hpp"
#include "note_sets.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using PitchClasses = DynamicArray<PitchClass, 8>;
using Octaves = DynamicArray<int8_t, 8>;

void minor_key1(PitchClasses& pitch_classes, Octaves& octaves, int key) {
  notes::minor_key1(pitch_classes, key);
  notes::center_biased_octave_set(octaves);
}

MIDINote sample_note(const PitchClasses& pitch_classes, const Octaves& octaves) {
  if (!pitch_classes.empty() && !octaves.empty()) {
    auto pc = pitch_classes[int(urand() * pitch_classes.size())];
    auto oct = octaves[int(urand() * octaves.size())];
    return MIDINote{pc, oct, 127};
  } else {
    return MIDINote::C3;
  }
}

} //  anon

NoteSetNode::NoteSetNode() {
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::MIDIMessage, this, 0});
}

InputAudioPorts NoteSetNode::inputs() const {
  return input_ports;
}

OutputAudioPorts NoteSetNode::outputs() const {
  return output_ports;
}

MIDINote NoteSetNode::render_get_randomized_note() const {
  PitchClasses pitch_classes;
  Octaves octaves;
  minor_key1(pitch_classes, octaves, key);
  return sample_note(pitch_classes, octaves);
}

void NoteSetNode::process(const AudioProcessData& in,
                          const AudioProcessData& out,
                          AudioEvents*,
                          const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == 1 && out.descriptors.size() == 1);

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message_in{};
    in.descriptors[0].read(in.buffer.data, i, &message_in);
    auto message_out = message_in;

    if (message_in.is_note_on()) {
      auto note = MIDINote::from_note_number(message_in.note_number());
      key = int(note.pitch_class);

      auto sampled_note = render_get_randomized_note();
      //  Transform the note number of the message.
      message_out.set_note_number(sampled_note.note_number());
    }

    out.descriptors[0].write(out.buffer.data, i, &message_out);
  }
}

GROVE_NAMESPACE_END
