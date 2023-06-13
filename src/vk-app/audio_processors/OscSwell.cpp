#include "OscSwell.hpp"
#include "signal.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Envelope::Params randomized_params() {
  Envelope::Params result{};
  result.attack_time = lerp(urand(), 1.0, 8.0);
  result.decay_time = lerp(urand(), 1.0, 8.0);
  result.sustain_time = lerp(urand(), 0.5, 1.0);
  result.release_time = 0.0;
  result.sustain_amp = 0.0;
  return result;
}

void left_shift(uint8_t* p, uint8_t n) {
  std::rotate(p, p + 1, p + n);
}

} //  anon

OscSwell::OscSwell(AudioParameterID node_id, const AudioScale* scale, bool enable_events) :
  node_id{node_id}, scale{scale}, events_enabled{enable_events} {
  //
  auto opt_flags = AudioPort::Flags::marked_optional();
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 1, opt_flags});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 1});

  for (auto& env : envelopes) {
    env.configure(randomized_params());
  }
  for (auto& osc : oscillators) {
    osc = osc::Sin{default_sample_rate(), frequency_a4()};
  }
  for (auto& note : active_notes) {
    note = midi_note_number_a4();
  }

  //  10ms
  input_gain.set_time_constant95(10e-3f);
}

InputAudioPorts OscSwell::inputs() const {
  return input_ports;
}

OutputAudioPorts OscSwell::outputs() const {
  return output_ports;
}

void OscSwell::process(const AudioProcessData& in, const AudioProcessData& out,
                       AudioEvents*, const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  for (int i = 0; i < num_voices; i++) {
    auto& env = envelopes[i];
    if (env.elapsed() && grove::urand() > 0.95) {
      if (num_pending_notes > 0) {
        active_notes[i] = pending_notes[0];
        left_shift(pending_notes, num_pending_notes);
        num_pending_notes--;
      }
      env.configure(randomized_params());
      env.note_on();
    }
  }
  for (int i = 0; i < num_voices; i++) {
    auto& osc = oscillators[i];
    osc.set_sample_rate(info.sample_rate);

    const Tuning* tuning = scale->render_get_tuning();
    double freq = note_number_to_frequency_equal_temperament(active_notes[i], *tuning);
    osc.set_frequency(freq);
  }

  const auto& in_note_desc = in.descriptors[0];
  const auto& in_gain_desc = in.descriptors[1];
  const bool in_gain_is_missing = in_gain_desc.is_missing();

  const auto& out_desc0 = out.descriptors[0];
  const auto& out_desc1 = out.descriptors[1];

  float latest_signal_val;
  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message;
    in_note_desc.read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      uint8_t note_number = message.note_number();
      if (num_pending_notes < num_voices) {
        pending_notes[num_pending_notes++] = note_number;
      } else {
        left_shift(pending_notes, num_pending_notes);
        pending_notes[num_pending_notes-1] = note_number;
      }
    }

    if (!in_gain_is_missing) {
      in_gain_desc.read(in.buffer.data, i, &input_gain.target);
    }

    const float in_gain = input_gain.tick(float(info.sample_rate));
    float s{};
    for (int v = 0; v < num_voices; v++) {
      s += in_gain * envelopes[v].tick(info.sample_rate) * oscillators[v].tick();
    }

    out_desc0.write(out.buffer.data, i, &s);
    out_desc1.write(out.buffer.data, i, &s);
    latest_signal_val = s;
  }

#if 1
  {
    float v{};
    if (audio::mean_signal_amplitude<64>(out.buffer, out.descriptors[0], info.num_frames, &v)) {
      const float min_db = -50.0f;
      const float max_db = 12.0f;
      v = (clamp(float(amplitude_to_db(v)), min_db, max_db) - min_db) / (max_db - min_db);
      latest_signal_val = v * float(num_voices);
    }
  }
#endif

  if (events_enabled && info.num_frames > 0) {
    auto stream = audio_event_system::default_event_stream();

    const int write_frame = info.num_frames - 1;
    const float signal_repr_value = signal_repr.clamp(
      std::abs(latest_signal_val) / float(num_voices));
    (void) audio_event_system::render_push_event(stream, make_monitorable_parameter_audio_event(
      {node_id, 0},
      make_float_parameter_value(signal_repr_value),
      write_frame, 0));
  }
}

void OscSwell::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  auto* dst = mem.push(1);
  dst[0] = signal_repr.make_descriptor(node_id, 0, 0.0f, "signal_representation", monitor_flags);
}

GROVE_NAMESPACE_END
