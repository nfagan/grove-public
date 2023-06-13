#include "PitchCVGen1.hpp"
#include "pitch_cv.hpp"
#include "note_sets.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr float min_pitch_cv_mod_depth_semitones() {
  return 0.2f;
}

constexpr float max_pitch_cv_mod_depth_semitones() {
  return 2.0f;
}

} //  anon

PitchCVGen1::PitchCVGen1(AudioParameterID node_id,
                         const Transport* transport,
                         const AudioParameterSystem* parameter_system) :
  node_id{node_id},
  transport{transport},
  parameter_system{parameter_system},
  pitch_cv{} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
}

void PitchCVGen1::process(const AudioProcessData& in,
                          const AudioProcessData& out,
                          AudioEvents*,
                          const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  pitch_cv_lfo.set_sample_rate(info.sample_rate);
  pitch_cv_lfo.set_frequency(5.0);

  ScoreCursor transport_cursor = transport->render_get_cursor_location();
  const TimeSignature tsig = reference_time_signature();
  const double bps = beats_per_sample_at_bpm(transport->get_bpm(), info.sample_rate, tsig);

  const auto cv_mod_depth_span = max_pitch_cv_mod_depth_semitones() -
                                 min_pitch_cv_mod_depth_semitones();

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  const auto self_changes = param_changes.view_by_parent(node_id);
  auto cv_depth_changes = self_changes.view_by_parameter(0);
  int cv_depth_index{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(cv_depth_changes, cv_depth_index, pitch_cv_mod_depth, i);

    MIDIMessage message;
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      double rem{};
      semitone_to_midi_note_components(
        message.semitone(), &center_pitch_class, &center_pitch_octave, &rem);
      (void) rem;
    }

    double quantum = audio::quantize_floor(
      transport_cursor.beat, audio::Quantization::Eighth, tsig.numerator);
    if (quantum != last_quantum) {
      //  randomize pitch
      last_quantum = quantum;

      const PitchClass pc_scale = notes::uniform_sample_minor_key2();
      auto eval_pitch_class = PitchClass((int(pc_scale) + int(center_pitch_class)) % 12);
      auto center_st = note_to_semitone(eval_pitch_class, center_pitch_octave);

      pitch_cv.target = float(audio::PitchCVMap::semitone_to_cv(center_st));
      pitch_cv.set_time_constant95(0.05f);
    }

    auto cv_mod_depth = pitch_cv_mod_depth.evaluate() * cv_mod_depth_span +
                        min_pitch_cv_mod_depth_semitones();
    auto cv_lfo_mod_amount = audio::PitchCVMap::semitone_to_cv(
      float(pitch_cv_lfo.tick()) * cv_mod_depth);
    const float cv = pitch_cv.tick(float(info.sample_rate)) + float(cv_lfo_mod_amount);
    out.descriptors[0].write(out.buffer.data, i, &cv);

    transport_cursor.wrapped_add_beats(bps, tsig.numerator);
  }
}

InputAudioPorts PitchCVGen1::inputs() const {
  return input_ports;
}

OutputAudioPorts PitchCVGen1::outputs() const {
  return output_ports;
}

void PitchCVGen1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto* dst = mem.push(1);
  dst[0] = pitch_cv_mod_depth.make_descriptor(node_id, 0, 0.0f, "pitch_cv_mod_depth");
}

GROVE_NAMESPACE_END
