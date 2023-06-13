#include "ModulatedOscillatorNode.hpp"
#include "../AudioScale.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

#define USE_FLOAT_OUTPUT (1)

ModulatedOscillatorNode::ModulatedOscillatorNode(const AudioScale* scale) :
  oscillator{44.1e3, frequency_a4()},
  scale{scale},
  current_note_number{midi_note_number_a4()},
  center_frequency{frequency_a4()} {
  //
  oscillator.fill_tri(4);
  oscillator.normalize();

  for (int i = 0; i < num_output_ports; i++) {
#if USE_FLOAT_OUTPUT
    OutputAudioPort signal(BufferDataType::Float, this, i);
#else
    OutputAudioPort signal(BufferDataType::Sample2, this, i);
#endif
    output_ports.push_back(signal);
  }

  InputAudioPort freq_mod(BufferDataType::Float, this, 0);
  input_ports.push_back(freq_mod);

  InputAudioPort gain_mod(BufferDataType::Float, this, 1);
  input_ports.push_back(gain_mod);

  InputAudioPort midi_note(BufferDataType::MIDIMessage, this, 2);
  input_ports.push_back(midi_note);

  freq_mod_depth = 5.0;
}

OutputAudioPorts ModulatedOscillatorNode::outputs() const {
  return output_ports;
}

InputAudioPorts ModulatedOscillatorNode::inputs() const {
  return input_ports;
}

void ModulatedOscillatorNode::process(const AudioProcessData& in,
                                      const AudioProcessData& out,
                                      AudioEvents*,
                                      const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == 3);

  auto freq_mod_descriptor = in.descriptors[0];
  auto gain_mod_descriptor = in.descriptors[1];
  auto midi_note_descriptor = in.descriptors[2];

  assert(freq_mod_descriptor.is_float() &&
         gain_mod_descriptor.is_float() &&
         midi_note_descriptor.is_midi_message());

  const auto* tuning = scale->render_get_tuning();
  oscillator.set_sample_rate(info.sample_rate);
  center_frequency = note_number_to_frequency_equal_temperament(current_note_number, *tuning);

  for (int i = 0; i < info.num_frames; i++) {
    float freq_mod;
    float gain_mod;
    MIDIMessage midi_note;

    freq_mod_descriptor.read<float>(in.buffer.data, i, &freq_mod);
    gain_mod_descriptor.read<float>(in.buffer.data, i, &gain_mod);
    midi_note_descriptor.read<MIDIMessage>(in.buffer.data, i, &midi_note);

    if (midi_note.is_note_on()) {
      current_note_number = midi_note.note_number();
      center_frequency = note_number_to_frequency_equal_temperament(current_note_number, *tuning);
    }
    oscillator.set_frequency(center_frequency + freq_mod * freq_mod_depth);

#if USE_FLOAT_OUTPUT
    auto scalar_sample = float(oscillator.tick() * (gain_mod * gain_mod_depth));

    for (auto& descr : out.descriptors) {
      assert(descr.is_float());
      descr.write<float>(out.buffer.data, i, &scalar_sample);
    }
#else
    auto scalar_sample = Sample(oscillator.tick() * (gain_mod * gain_mod_depth));
    Sample2 sample;
    sample.assign(scalar_sample);

    for (auto& descr : out.descriptors) {
      assert(descr.is_sample2());
      descr.write<Sample2>(out.buffer.data, i, &sample);
    }
#endif
  }
}

#undef USE_FLOAT_OUTPUT

GROVE_NAMESPACE_END


