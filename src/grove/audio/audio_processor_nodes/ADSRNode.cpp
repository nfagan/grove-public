#include "ADSRNode.hpp"
#include "grove/common/common.hpp"
#include <iostream>

GROVE_NAMESPACE_BEGIN

ADSRNode::ADSRNode() :
  envelope(44.1e3, Envelope::Params{}),
  num_notes_on(0),
  pending_note_off(false) {
  //
  env::ADSR::default_configure1(envelope);

  for (int i = 0; i < num_output_channels; i++) {
    OutputAudioPort output_port(BufferDataType::Float, this, i);
    output_ports.push_back(output_port);
  }

  InputAudioPort input_port(BufferDataType::MIDIMessage, this, 0);
  input_ports.push_back(input_port);
}

InputAudioPorts ADSRNode::inputs() const {
  return input_ports;
}

OutputAudioPorts ADSRNode::outputs() const {
  return output_ports;
}

void ADSRNode::process(const AudioProcessData& in,
                       const AudioProcessData& out,
                       AudioEvents*,
                       const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors[0].is_midi_message());

  envelope.set_sample_rate(info.sample_rate);

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message;
    in.descriptors[0].read<MIDIMessage>(in.buffer.data, i, &message);

    if (message.is_note_on()) {
      envelope.note_on();
      num_notes_on++;
      pending_note_off = true;

    } else if (message.is_note_off()) {
      num_notes_on--;
    }

    if (num_notes_on < 0) {
      num_notes_on = 0;
    }

    if (pending_note_off && num_notes_on == 0) {
      envelope.note_off();
      pending_note_off = false;
    }

    auto amp = float(envelope.tick());

    for (int j = 0; j < out.descriptors.size(); j++) {
      assert(out.descriptors[j].is_float());
      out.descriptors[j].write(out.buffer.data, i, &amp);
    }
  }
}

GROVE_NAMESPACE_END
