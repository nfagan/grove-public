#include "EnvelopeSequencer.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

EnvelopeSequencer::EnvelopeSequencer(const Transport* transport) :
  transport{transport} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::Sample2, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Sample2, this, 0});

  Envelope::Params env_params{};
  env_params.attack_time = 0.1;
  env_params.decay_time = 0.5;
  env_params.sustain_time = 0.0;
  env_params.sustain_amp = 1.0;
  env_params.release_time = 0.5;
  env_params.infinite_sustain = false;
//  envelope.configure(env_params);

  for (auto& env : envelopes) {
    env.configure(env_params);
  }

//  std::fill(step_gains.begin(), step_gains.end(), 1.0f);
}

void EnvelopeSequencer::process(const AudioProcessData& in,
                                const AudioProcessData& out,
                                AudioEvents*,
                                const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == 1 && in.descriptors[0].is_sample2());
  assert(out.descriptors.size() == 1 && out.descriptors[0].is_sample2());

  const auto& in0 = in.descriptors[0];
  const auto& out0 = out.descriptors[0];

  const auto time_signature = reference_time_signature();
  auto bps = beats_per_sample_at_bpm(transport->get_bpm(), info.sample_rate, time_signature);

  if (transport->just_played()) {
    cursor.zero();
    step_index = -1;
  }

  const auto beat_div = audio::beat_divisor(quantization);

  for (int i = 0; i < info.num_frames; i++) {
    auto is_zero = cursor == ScoreCursor{};
    auto last_beat = std::floor(cursor.to_beats(time_signature.numerator) * beat_div);
    cursor.wrapped_add_beats(bps, time_signature.numerator);
    auto curr_beat = std::floor(cursor.to_beats(time_signature.numerator) * beat_div);
    const bool is_new_note = is_zero || curr_beat != last_beat;

    if (is_new_note && transport->render_is_playing()) {
//      envelope.note_on();
      step_index = (step_index + 1) % num_steps;
      envelopes[step_index].note_on();
    }

    float env_val = 0.0f;
    for (auto& env : envelopes) {
      env_val += 1.0f / float(num_steps) * env.tick(info.sample_rate);
    }

    Sample2 src;
    in0.read(in.buffer.data, i, &src);
    src = src * env_val;
    out0.write(out.buffer.data, i, &src);
  }
}

InputAudioPorts EnvelopeSequencer::inputs() const {
  return input_ports;
}

OutputAudioPorts EnvelopeSequencer::outputs() const {
  return output_ports;
}

GROVE_NAMESPACE_END