#include "Skittering1.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename P>
Optional<int> check_apply_int_param(P& p, const AudioParameterChangeView& param_changes) {
  AudioParameterChange change{};
  if (param_changes.collapse_to_last_change(&change)) {
    p.apply(change);
    return Optional<int>(p.evaluate());
  }
  return NullOpt{};
}

template <typename P>
void check_apply_float_param(P& p, const AudioParameterChangeView& param_changes) {
  AudioParameterChange change{};
  if (param_changes.collapse_to_last_change(&change)) {
    p.apply(change);
  }
}

} //  anon

Skittering1::Skittering1(
  uint32_t node_id, const AudioBufferStore* buff_store, const Transport* transport,
  const AudioScale* scale, const AudioParameterSystem* param_sys,
  uint32_t pitch_sample_group, AudioBufferHandle buff_handle) :
  //
  node_id{node_id}, buffer_store{buff_store}, transport{transport}, scale{scale},
  param_sys{param_sys}, pitch_sample_group{pitch_sample_group}, buffer_handle{buff_handle} {
  //
  for (int i = 0; i < num_voices; i++) {
    voices[i].st_phase = i;
    voices[i].arp = i < num_voices/2;
  }

  global_semitone_offset = -4.0f + float(midi_note_number_a4());
}

void Skittering1::process(
  const AudioProcessData& in, const AudioProcessData& out,
  AudioEvents*, const AudioRenderInfo& info) {
  //
  auto buff_chunk = buffer_store->render_get(buffer_handle, 0, 0);
  if (!buff_chunk || !buff_chunk.value().descriptor.is_n_channel_float(2) ||
      buff_chunk.value().empty()) {
    return;
  }

  {
    const auto& changes = param_system::render_read_changes(param_sys);
    auto self_changes = changes.view_by_parent(node_id);
    uint32_t pi{};
    (void) check_apply_int_param(params.prefer_midi_input, self_changes.view_by_parameter(pi++));
    check_apply_float_param(params.arp_mix, self_changes.view_by_parameter(pi++));
    check_apply_float_param(params.overall_gain, self_changes.view_by_parameter(pi++));
  }

  auto& buff = buff_chunk.value();

#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
#else
  const auto& tuning = *scale->render_get_tuning();
#endif

  double st_sample_set[16];
  st_sample_set[0] = 0.0;
  int num_st_samples = std::max(1, pss::render_read_semitones(
    pss::get_global_pitch_sampling_system(),
    PitchSampleSetGroupHandle{pitch_sample_group}, 0, st_sample_set, 16));

  const bool prefer_midi = params.prefer_midi_input.value;

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    in.descriptors[0].read(in.buffer.data, i, &message);

    const float arp_mix = params.arp_mix.evaluate();
    const float overall_gain = params.overall_gain.evaluate();

    if (message.is_note_on()) {
      const float midi_st = float(message.note_number()) - global_semitone_offset;
      for (auto& v : voices) {
        v.pending_midi_st = midi_st;
        if (v.arp) {
          v.curr_midi_st = midi_st;
        }
      }
    }

    const int fi_quant = transport->render_get_pausing_cursor_quantized_event_frame_offset(
      audio::Quantization::Sixteenth);
    const bool new_start = i == fi_quant;

    float s0{};
    float s1{};
    for (auto& voice : voices) {
      if (voice.arp && new_start) {
        voice.st_phase = voice.st_phase + 1;
      }
      voice.st_phase %= num_st_samples;

      double st = global_semitone_offset;
      if (prefer_midi) {
        st += voice.curr_midi_st;
      } else {
        st += st_sample_set[voice.st_phase];
      }

      double& fi = voice.fi;
      if (uint64_t(fi) >= buff.frame_end()) {
        fi = 0.0;
        voice.curr_midi_st = voice.pending_midi_st;
      }

      auto lerp_info = util::make_linear_interpolation_info(fi, buff.frame_end());
      float st0 = util::tick_interpolated_float(buff.data, buff.channel_descriptor(0), lerp_info);
      float st1 = util::tick_interpolated_float(buff.data, buff.channel_descriptor(1), lerp_info);

      const float g = 1.0f / float(num_voices) * (voice.arp ? arp_mix : 1.0f);
      s0 += g * st0 * 2.0f;
      s1 += g * st1 * 2.0f;

#if GROVE_PREFER_AUDIO_SCALE_SYS
      const double rm = scale_system::render_get_rate_multiplier_from_semitone(scale_sys, st, i);
      (void) scale;
#else
      const double rm = semitone_to_rate_multiplier_equal_temperament(st, tuning);
#endif
      fi += frame_index_increment(buff.descriptor.sample_rate, info.sample_rate, rm);
    }

    s0 *= overall_gain;
    s1 *= overall_gain;

    out.descriptors[0].write(out.buffer.data, i, &s0);
    out.descriptors[1].write(out.buffer.data, i, &s1);
  }
}

void Skittering1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  int di{};
  uint32_t pi{};
  Params p{};
  auto* dst = mem.push(Params::num_params);
  dst[di++] = p.prefer_midi_input.make_descriptor(
    node_id, pi++, p.prefer_midi_input.value, "prefer_midi_input");
  dst[di++] = p.arp_mix.make_descriptor(node_id, pi++, p.arp_mix.value, "arp_mix");
  dst[di++] = p.overall_gain.make_descriptor(node_id, pi++, p.overall_gain.value, "overall_gain");
}

InputAudioPorts Skittering1::inputs() const {
  InputAudioPorts result;
  int pi{};
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<Skittering1*>(this), pi++});
  return result;
}

OutputAudioPorts Skittering1::outputs() const {
  OutputAudioPorts result;
  int pi{};
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<Skittering1*>(this), pi++});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<Skittering1*>(this), pi++});
  return result;
}

GROVE_NAMESPACE_END
