#include "SteerableSynth1.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
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

SteerableSynth1::SteerableSynth1(
  uint32_t node_id, const AudioParameterSystem* param_sys, const AudioScale* scale,
  uint32_t pitch_sample_group_id) :
  node_id{node_id}, parameter_system{param_sys}, scale{scale},
  pitch_sample_group_id{pitch_sample_group_id} {
  //
  for (auto& env : envelopes) {
    env.configure(randomized_params());
  }
  for (auto& osc : oscillators) {
    osc = osc::Sin{default_sample_rate(), frequency_a4()};
  }
  for (auto& note : active_notes) {
    note = midi_note_number_a4();
  }

  pitch_bend.set_time_constant95(1.0f);
  amp_mod_gain.set_time_constant95(5e-3f);
  noise_osc.fill_white_noise();
  noise_osc.set_frequency(5.0);
  latest_note_number = midi_note_number_a4();
}

void SteerableSynth1::process(const AudioProcessData& in, const AudioProcessData& out,
                              AudioEvents*, const AudioRenderInfo& info) {
  assert(in.descriptors.size() == 2 && out.descriptors.size() == 2);

  reverb.set_sample_rate(info.sample_rate);
  noise_osc.set_sample_rate(info.sample_rate);
  noise_amp_lfo.set_sample_rate(info.sample_rate);
  noise_amp_lfo.set_frequency(0.05);

  {
    const auto& changes = param_system::render_read_changes(parameter_system);
    auto self_changes = changes.view_by_parent(node_id);
    auto pitch_changes = self_changes.view_by_parameter(0);
    auto rev_mix_changes = self_changes.view_by_parameter(1);
    auto noise_gain_changes = self_changes.view_by_parameter(2);

    AudioParameterChange pitch_change{};
    if (pitch_changes.collapse_to_last_change(&pitch_change)) {
      pitch_bend_param.apply(pitch_change);
    }
    AudioParameterChange rev_change{};
    if (rev_mix_changes.collapse_to_last_change(&rev_change)) {
      reverb_mix.apply(rev_change);
    }
    AudioParameterChange noise_change{};
    if (noise_gain_changes.collapse_to_last_change(&noise_change)) {
      noise_gain.apply(noise_change);
    }
  }

  for (int i = 0; i < num_voices; i++) {
    auto& env = envelopes[i];
    if (env.elapsed() && grove::urand() > 0.95) {
      if (num_pending_notes > 0) {
        active_notes[i] = pending_notes[0];
        left_shift(pending_notes, num_pending_notes);
        num_pending_notes--;
      }
#if 1
      double st_off = pss::render_uniform_sample_semitone(
        pss::get_global_pitch_sampling_system(),
        PitchSampleSetGroupHandle{pitch_sample_group_id}, 0, 0.0);
      active_notes[i] = uint8_t(clamp(double(latest_note_number) + st_off, 0.0, 255.0));
#endif
      env.configure(randomized_params());
      env.note_on();
    }
  }
  for (int i = 0; i < num_voices; i++) {
    auto& osc = oscillators[i];
    osc.set_sample_rate(info.sample_rate);
  }

#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
  (void) scale;
#else
  const Tuning* tuning = scale->render_get_tuning();
#endif

  const auto& in_note_desc = in.descriptors[0];
  const auto& amp_mod_desc = in.descriptors[1];
  const auto& out_desc0 = out.descriptors[0];
  const auto& out_desc1 = out.descriptors[1];

  float latest_signal_val;
  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message;
    in_note_desc.read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      uint8_t note_number = message.note_number();
      latest_note_number = note_number;
      if (num_pending_notes < num_voices) {
        pending_notes[num_pending_notes++] = note_number;
      } else {
        left_shift(pending_notes, num_pending_notes);
        pending_notes[num_pending_notes-1] = note_number;
      }
    }

    float amp_mod_target{1.0f};
    if (!amp_mod_desc.is_missing()) {
      amp_mod_desc.read(in.buffer.data, i, &amp_mod_target);
    }
    amp_mod_gain.set_target(amp_mod_target);
    const float amp_mod = amp_mod_gain.tick(float(info.sample_rate));

    pitch_bend.set_target(pitch_bend_param.evaluate());
    const float pb_amt = pitch_bend.tick(float(info.sample_rate)) * 5.0f;  //  +/- semitones

    float s{};
    for (int v = 0; v < num_voices; v++) {
#if GROVE_PREFER_AUDIO_SCALE_SYS
      double freq = scale_system::render_get_frequency_from_semitone(
        scale_sys, note_number_to_semitone(active_notes[v]) + pb_amt, i);
#else
      double freq = semitone_to_frequency_equal_temperament(
        note_number_to_semitone(active_notes[v]) + pb_amt, *tuning);
#endif
      oscillators[v].set_frequency(freq);
      s += envelopes[v].tick(info.sample_rate) * oscillators[v].tick();
    }

    float gain_adj = std::max(0.0f, noise_amp_lfo.tick() * 0.1f);
    s += (noise_gain.evaluate() + gain_adj) * noise_osc.tick() * 0.2f;

    const Sample2 src{s, s};
    Sample2 s2 = reverb.tick(
      src, info.sample_rate, Reverb1::FDNFeedbackLimits::min, reverb_mix.evaluate());
    s2 = s2 * amp_mod;

    out_desc0.write(out.buffer.data, i, &s2.samples[0]);
    out_desc1.write(out.buffer.data, i, &s2.samples[1]);
    latest_signal_val = s;
  }
}

InputAudioPorts SteerableSynth1::inputs() const {
  auto opt = AudioPort::Flags::marked_optional();
  InputAudioPorts input_ports;
  input_ports.push_back({BufferDataType::MIDIMessage, const_cast<SteerableSynth1*>(this), 0});
  input_ports.push_back({BufferDataType::Float, const_cast<SteerableSynth1*>(this), 1, opt});
  return input_ports;
}

OutputAudioPorts SteerableSynth1::outputs() const {
  OutputAudioPorts output_ports;
  output_ports.push_back({BufferDataType::Float, const_cast<SteerableSynth1*>(this), 0});
  output_ports.push_back({BufferDataType::Float, const_cast<SteerableSynth1*>(this), 1});
  return output_ports;
}

void SteerableSynth1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  AudioParameterDescriptor* dst = mem.push(3);
  int p{};
  dst[0] = pitch_bend_param.make_descriptor(node_id, p++, 0.0f, "pitch_bend");
  dst[1] = reverb_mix.make_descriptor(node_id, p++, 0.0f, "reverb_mix");
  dst[2] = noise_gain.make_descriptor(node_id, p++, 0.0f, "noise_gain");
}

GROVE_NAMESPACE_END
