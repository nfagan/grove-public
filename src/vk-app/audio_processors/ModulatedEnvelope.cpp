#include "ModulatedEnvelope.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

ModulatedEnvelope::ModulatedEnvelope() {
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});

  reference_params.attack_time = 0.5;
  reference_params.decay_time = 0.5;
  reference_params.sustain_time = 0.0;
  reference_params.sustain_amp = 1.0;
  reference_params.release_time = 0.5;
  reference_params.infinite_sustain = false;

  current_params = reference_params;
}

void ModulatedEnvelope::process(const AudioProcessData& in,
                                const AudioProcessData& out,
                                AudioEvents*,
                                const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  lfo.set_sample_rate(info.sample_rate);
  lfo.set_frequency(0.025);

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message;
    in.descriptors[0].read(in.buffer.data, i, &message);

    auto lfo_val = lfo.tick();
    current_params.attack_time = std::max(0.05, lfo_val * 0.45 + reference_params.attack_time);
    current_params.decay_time = std::max(0.05, lfo_val * 0.45 + reference_params.decay_time);

    envelope.configure(current_params);

    if (message.is_note_on()) {
      envelope.note_on();

    } else if (message.is_note_off()) {
      envelope.note_off();
    }

    float env_val = envelope.tick(info.sample_rate);
    out.descriptors[0].write(out.buffer.data, i, &env_val);
  }
}

InputAudioPorts ModulatedEnvelope::inputs() const {
  return input_ports;
}

OutputAudioPorts ModulatedEnvelope::outputs() const {
  return output_ports;
}

GROVE_NAMESPACE_END