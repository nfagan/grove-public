#include "MultiComponentSampler.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr double min_grain_dur_s = 25e-3;
  static constexpr double max_grain_dur_s = 1000e-3;
};

double quantization_to_duration(audio::Quantization quant, double bpm, double tsig_num) {
  return 1.0 / bpm_to_bps(bpm) / audio::quantization_divisor(quant) * tsig_num;
}

audio::Quantization float01_to_quantization(float t) {
  if (t < 0.125f) {
    return audio::Quantization::SixtyFourth;
  } else if (t < 0.25f) {
    return audio::Quantization::ThirtySecond;
  } else if (t < 0.5f) {
    return audio::Quantization::Sixteenth;
  } else if (t < 0.75f) {
    return audio::Quantization::Eighth;
  } else {
    return audio::Quantization::Quarter;
  }
}

void note_set0(double* offsets, int* num_st_offsets) {
  *num_st_offsets = 0;
  offsets[(*num_st_offsets)++] = 0.0;
  offsets[(*num_st_offsets)++] = -12.0;
  offsets[(*num_st_offsets)++] = 12.0;
}

void note_set1(double* offsets, int* num_st_offsets) {
  //  5, 7, -12+5, -12+7
  *num_st_offsets = 0;
  offsets[(*num_st_offsets)++] = 0.0;
  offsets[(*num_st_offsets)++] = 5.0;
  offsets[(*num_st_offsets)++] = 7.0;
  offsets[(*num_st_offsets)++] = 12.0;
  offsets[(*num_st_offsets)++] = -12.0;
  offsets[(*num_st_offsets)++] = -7.0;
  offsets[(*num_st_offsets)++] = -5.0;
}

void note_set2(double* offsets, int* num_st_offsets) {
  //  2, 5, 7, 9, -12+2, -12+5, -12+7, -12+9
  *num_st_offsets = 0;
  offsets[(*num_st_offsets)++] = 0.0;
  offsets[(*num_st_offsets)++] = 2.0;
  offsets[(*num_st_offsets)++] = 5.0;
  offsets[(*num_st_offsets)++] = 7.0;
  offsets[(*num_st_offsets)++] = 9.0;
  offsets[(*num_st_offsets)++] = -10.0;
  offsets[(*num_st_offsets)++] = -7.0;
  offsets[(*num_st_offsets)++] = -5.0;
  offsets[(*num_st_offsets)++] = -3.0;
}

void note_set3(double* offsets, int* num_st_offsets) {
  note_set2(offsets, num_st_offsets);
  offsets[(*num_st_offsets)++] = -12.0;
  offsets[(*num_st_offsets)++] = 12.0;
}

void note_set(double* offsets, int* num_st_offsets, int i) {
  switch (i) {
    case 0: {
      note_set0(offsets, num_st_offsets);
      break;
    }
    case 1: {
      note_set1(offsets, num_st_offsets);
      break;
    }
    case 2: {
      note_set2(offsets, num_st_offsets);
      break;
    }
    case 3: {
      note_set3(offsets, num_st_offsets);
      break;
    }
  }
}

} //  anon

MultiComponentSampler::MultiComponentSampler(
  uint32_t node_id, const AudioBufferStore* buffer_store,
  const AudioBufferHandle* buff_handles, int num_handles, const AudioScale* scale,
  const Transport* transport, const AudioParameterSystem* param_sys, uint32_t pitch_sample_group) :
  node_id{node_id}, buffer_store{buffer_store}, scale{scale}, transport{transport}, param_sys{param_sys},
  pitch_sample_group{pitch_sample_group} {
  //
  if (num_handles > 0) {
    for (int i = 0; i < max_num_voices; i++) {
      this->buff_handles[i] = buff_handles[i % num_handles];
    }
    num_buff_handles = max_num_voices;
  }

  note_set(global_possible_st_offsets, &num_global_st_offsets, params.note_set_index.value);

  global_grain_dur = Config::min_grain_dur_s;
  kb_semitone = note_number_to_semitone(midi_note_number_a4()) + 9.0;
  voice_delay0 = audio::InterpolatedDelayLine<float>{default_sample_rate(), 0.2};
  voice_delay1 = audio::InterpolatedDelayLine<float>{default_sample_rate(), 0.2};
}

void MultiComponentSampler::process(
  const AudioProcessData& in, const AudioProcessData& out, AudioEvents*,
  const AudioRenderInfo& info) {
  //
  AudioBufferChunk chunks[max_num_voices]{};
  int num_chunks{};
  for (int i = 0; i < num_buff_handles; i++) {
    if (auto chunk = buffer_store->render_get(buff_handles[i], 0, 0)) {
      if (chunk.value().descriptor.is_n_channel_float(2) && chunk.value().frame_size > 0) {
        chunks[num_chunks++] = chunk.value();
      }
    }
  }

  if (num_chunks == 0) {
    return;
  }

  constexpr int global_note_set_param_val = 0;

  { //  params
    const auto& changes = param_system::render_read_changes(param_sys);
    auto self_changes = changes.view_by_parent(node_id);
    decltype(&params.granule_dur) float_ps[5]{
      &params.granule_dur, &params.voice_delay_mix, &params.p_sin, &params.p_masked_out,
      &params.p_quantized_granule_dur};

    uint32_t pi{};
    for (auto* p : float_ps) {
      auto param_changes = self_changes.view_by_parameter(pi++);
      AudioParameterChange change{};
      if (param_changes.collapse_to_last_change(&change)) {
        p->apply(change);
      }
    }

    const int curr_set = params.note_set_index.value;
    decltype(&params.note_set_index) int_ps[1]{
      &params.note_set_index
    };
    for (auto* p : int_ps) {
      auto param_changes = self_changes.view_by_parameter(pi++);
      AudioParameterChange change{};
      if (param_changes.collapse_to_last_change(&change)) {
        p->apply(change);
      }
    }
    {
      int new_set = params.note_set_index.evaluate();
      if (curr_set != new_set && new_set != global_note_set_param_val) {
        assert(new_set > 0);
        note_set(global_possible_st_offsets, &num_global_st_offsets, new_set - 1);
      }
    }
  }

  if (params.note_set_index.value == global_note_set_param_val) {
    num_global_st_offsets = pss::render_read_semitones(
      pss::get_global_pitch_sampling_system(),
      PitchSampleSetGroupHandle{pitch_sample_group}, 0, global_possible_st_offsets, 16);
  }

  const double bpm = transport->get_bpm();

#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
  (void) scale;
#else
  const auto& tuning = *scale->render_get_tuning();
#endif
  const int fade_in_samples = std::max(1, int(std::floor(5e-3 * info.sample_rate)));

  for (int i = 0; i < info.num_frames; i++) {
    const float gran_dur_t = clamp01(params.granule_dur.evaluate());
    global_quantized_grain_dur = float01_to_quantization(gran_dur_t);
    global_grain_dur = lerp(gran_dur_t, Config::min_grain_dur_s, Config::max_grain_dur_s);
    const float voice_delay_mix = params.voice_delay_mix.evaluate();
    const float p_use_sin = params.p_sin.evaluate();
    const float p_mask_out = params.p_masked_out.evaluate();
    const float p_quantized_granule_dur = params.p_quantized_granule_dur.evaluate();

    MIDIMessage message;
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      //  +9 here is a hack because the samples we tend to use are c3, whereas the rest of the
      //  app uses a reference of a4. if we just used samples that were a4 to begin with,
      //  this wouldn't be necessary
      kb_semitone = note_number_to_semitone(message.note_number()) + 9.0;
    }

    float gran_sample0{};
    float gran_sample1{};

    for (Voice& voice : voices) {
      GranulatorVoice& gran = voice.granulator;

      if (gran.quantized_state == 1) {
        const int fi = transport->render_get_pausing_cursor_quantized_event_frame_offset(
          global_quantized_grain_dur);
        if (i == fi) {
          gran.quantized_state = 2;
        } else {
          continue;
        }
      }

      int& yi = gran.yi;
      assert(yi < max_num_voices);
      int& gi = gran.gi;
      double& fi = gran.fi;

      const auto* chunk = &chunks[yi];
      if (uint64_t(fi) >= chunk->frame_end()) { //  expired granule, choose a new one
        yi = int(urand() * num_chunks);
        chunk = &chunks[yi];
        fi = double(chunk->frame_offset) + double(chunk->frame_size) * urand();
        gi = 0;
      }

      auto lerp_info = util::make_linear_interpolation_info(fi, chunk->frame_end());
      float s0 = util::tick_interpolated_float(chunk->data, chunk->channel_descriptor(0), lerp_info);
      float s1 = util::tick_interpolated_float(chunk->data, chunk->channel_descriptor(1), lerp_info);

      float fade_gain = float(std::min(gi, gran.fade_in_sample_size)) / float(
        std::max(1, gran.fade_in_sample_size));
      fade_gain /= float(max_num_voices);
      s0 *= fade_gain;
      s1 *= fade_gain;

      const double st_off = kb_semitone + gran.st_offset + gran.st_noise;
#if GROVE_PREFER_AUDIO_SCALE_SYS
      const double rm = scale_system::render_get_rate_multiplier_from_semitone(scale_sys, st_off, i);
#else
      const double rm = semitone_to_rate_multiplier_equal_temperament(st_off, tuning);
#endif
      fi += frame_index_increment(chunk->descriptor.sample_rate, info.sample_rate, rm);

#if GROVE_PREFER_AUDIO_SCALE_SYS
      const double freq = scale_system::render_get_frequency_from_semitone(scale_sys, st_off - 9.0, i);
#else
      const double freq = semitone_to_frequency_equal_temperament(st_off - 9.0, tuning);
#endif
      const auto sin_v = fade_gain * float(
        osc::Sin::tick(info.sample_rate, &gran.sin_phase, freq));

      const float mask_gain = 1.0f - float(gran.masked_out);
      gran_sample0 += lerp(float(gran.use_sin), s0, sin_v) * mask_gain;
      gran_sample1 += lerp(float(gran.use_sin), s1, sin_v) * mask_gain;

      gi++;
      if (gi >= gran.sample_size) {
        //  Next granule
        gi = 0;
        { //  granule size
          gran.quantized_state = 0;
          double center_dur = global_grain_dur;
          double noise_prop = global_grain_dur_noise_prop;
          if (urand() < p_quantized_granule_dur) {
            center_dur = quantization_to_duration(
              global_quantized_grain_dur, bpm, reference_time_signature().numerator);
            noise_prop = 0.0;
            gran.quantized_state = 1; //  awaiting onset
          }
          double num_samples = info.sample_rate * (
            (urand() * 2.0 - 1.0) * center_dur * noise_prop + center_dur);
          gran.sample_size = int(std::max(1.0, std::floor(num_samples)));
          gran.fade_in_sample_size = std::min(gran.sample_size, fade_in_samples);
        }
        if (num_global_st_offsets > 0) {  //  semitone offset
          gran.st_offset = global_possible_st_offsets[int(urand() * num_global_st_offsets)];
        }
        { //  granule buffer
          yi = int(urand() * num_chunks);
          chunk = &chunks[yi];
          fi = double(chunk->frame_offset) + double(chunk->frame_size) * urand();
        }
        { //  sin
          gran.use_sin = urand() < p_use_sin;
        }
        { //  mask out
          gran.masked_out = urand() < p_mask_out;
        }
      }
    }

#if 1
    gran_sample0 = lerp(
      voice_delay_mix, gran_sample0, voice_delay0.tick(gran_sample0, 0.15, info.sample_rate, 0.9));
    gran_sample1 = lerp(
      voice_delay_mix, gran_sample1, voice_delay1.tick(gran_sample1, 0.175, info.sample_rate, 0.9));
#endif

    float gain_mod{1.0f};
    if (!in.descriptors[1].is_missing()) {
      in.descriptors[1].read(in.buffer.data, i, &gain_mod);
    }

    gran_sample0 *= gain_mod * 1.25f;
    gran_sample1 *= gain_mod * 1.25f;

    out.descriptors[0].write(out.buffer.data, i, &gran_sample0);
    out.descriptors[1].write(out.buffer.data, i, &gran_sample1);
  }
}

void MultiComponentSampler::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  Params p{};
  AudioParameterDescriptor* dst = mem.push(Params::num_params);
  uint32_t id{};
  int i{};
  dst[i++] = p.granule_dur.make_descriptor(node_id, id++, p.granule_dur.value, "granule_dur");
  dst[i++] = p.voice_delay_mix.make_descriptor(node_id, id++, p.voice_delay_mix.value, "voice_delay_mix");
  dst[i++] = p.p_sin.make_descriptor(node_id, id++, p.p_sin.value, "p_sin");
  dst[i++] = p.p_masked_out.make_descriptor(
    node_id, id++, p.p_masked_out.value, "p_masked_out");
  dst[i++] = p.p_quantized_granule_dur.make_descriptor(
    node_id, id++, p.p_quantized_granule_dur.value, "p_quantized_granule_dur");
  dst[i++] = p.note_set_index.make_descriptor(
    node_id, id++, p.note_set_index.value, "note_set_index");
}

InputAudioPorts MultiComponentSampler::inputs() const {
  InputAudioPorts result{};
  auto opt_flag = AudioPort::Flags::marked_optional();
  int pi{};
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<MultiComponentSampler*>(this), pi++});
  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<MultiComponentSampler*>(this), pi++, opt_flag});
  return result;
}

OutputAudioPorts MultiComponentSampler::outputs() const {
  OutputAudioPorts result{};
  int pi{};
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<MultiComponentSampler*>(this), pi++});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<MultiComponentSampler*>(this), pi++});
  return result;
}

GROVE_NAMESPACE_END
