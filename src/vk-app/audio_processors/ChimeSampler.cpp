#include "ChimeSampler.hpp"
#include "grove/audio/arpeggio.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/PitchSamplingSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/Vec2.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr Vec2f longer_on_time_lims = Vec2f{4.0f, 6.0f};
  static constexpr Vec2f longer_decay_time_lims = Vec2f{3.0f, 5.0f};
  static constexpr Vec2f longer_event_time_lims = Vec2f{5.0f, 10.0f};

  static constexpr Vec2f long_on_time_lims = Vec2f{2.0f, 4.0f};
  static constexpr Vec2f long_decay_time_lims = Vec2f{1.0f, 2.0f};
  static constexpr Vec2f long_event_time_lims = Vec2f{2.0f, 5.0f};

  static constexpr Vec2f med_on_time_lims = Vec2f{2.0f * 0.25f, 4.0f * 0.25f};
  static constexpr Vec2f med_decay_time_lims = Vec2f{1.0f * 0.25f, 2.0f * 0.25f};
  static constexpr Vec2f med_event_time_lims = Vec2f{2.0f * 0.25f, 5.0f * 0.25f};

  static constexpr Vec2f short_on_time_lims = Vec2f{2.0f * 0.125f, 4.0f * 0.125f};
  static constexpr Vec2f short_decay_time_lims = Vec2f{1.0f * 0.125f, 2.0f * 0.125f};
  static constexpr Vec2f short_event_time_lims = Vec2f{2.0f * 0.125f, 5.0f * 0.125f};

  static constexpr Vec2<double> med_time_left_lims = Vec2<double>{100.0e-3, 200.0e-3};
  static constexpr Vec2<double> long_time_left_lims = Vec2<double>{200.0e-3, 300.0e-3};

  static constexpr Vec2f gain_lims = Vec2f{0.25f, 1.0f};
  static constexpr int buffer_set1_offset = 2;
};

Vec2<double> get_duration_indexed_time_left_limits(int di) {
  switch (di) {
    case 0:
    case 1:
    case 2:
      return Config::med_time_left_lims;
    case 3:
      return Config::long_time_left_lims;
    default:
      return Config::med_time_left_lims;
  }
}

audio::Quantization get_duration_indexed_quantization(int di) {
  switch (di) {
    case 0:
      return audio::Quantization::Measure;
    case 1:
      return audio::Quantization::Half;
    case 2:
      return audio::Quantization::Quarter;
    default:
      return audio::Quantization::Measure;
  }
}

void get_duration_indexed_time_limits(
  int di, Vec2f* on_time_lims, Vec2f* decay_time_lims, Vec2f* event_time_lims) {
  //
  switch (di) {
    case 0: {
      *on_time_lims = Config::long_on_time_lims;
      *decay_time_lims = Config::long_decay_time_lims;
      *event_time_lims = Config::long_event_time_lims;
      break;
    }
    case 1: {
      *on_time_lims = Config::med_on_time_lims;
      *decay_time_lims = Config::med_decay_time_lims;
      *event_time_lims = Config::med_event_time_lims;
      break;
    }
    case 2: {
      *on_time_lims = Config::short_on_time_lims;
      *decay_time_lims = Config::short_decay_time_lims;
      *event_time_lims = Config::short_event_time_lims;
      break;
    }
    case 3: {
      *on_time_lims = Config::longer_on_time_lims;
      *decay_time_lims = Config::longer_decay_time_lims;
      *event_time_lims = Config::longer_event_time_lims;
      break;
    }
    default: {
      assert(false);
    }
  }
}

[[maybe_unused]]
void note_set1(double* offsets, int* num_st_offsets) {
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

void note_set2(double* offsets, int* num_st_offsets) {
  //  2, 5, 7, 9, -12+2, -12+5, -12+7, -12+9
  *num_st_offsets = 0;
  offsets[(*num_st_offsets)++] = 0.0;
  offsets[(*num_st_offsets)++] = 2.0;
  offsets[(*num_st_offsets)++] = 5.0;
  offsets[(*num_st_offsets)++] = 7.0;
  offsets[(*num_st_offsets)++] = 9.0;

  offsets[(*num_st_offsets)++] = 0.0 - 12.0;
  offsets[(*num_st_offsets)++] = 2.0 - 12.0;
  offsets[(*num_st_offsets)++] = 5.0 - 12.0;
  offsets[(*num_st_offsets)++] = 7.0 - 12.0;
  offsets[(*num_st_offsets)++] = 9.0 - 12.0;

  offsets[(*num_st_offsets)++] = 0.0 + 12.0;
  offsets[(*num_st_offsets)++] = 2.0 + 12.0;
  offsets[(*num_st_offsets)++] = 5.0 + 12.0;
  offsets[(*num_st_offsets)++] = 7.0 + 12.0;
  offsets[(*num_st_offsets)++] = 9.0 + 12.0;
}

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

ChimeSampler::ChimeSampler(
  uint32_t node_id, const AudioBufferStore* buff_store,
  const AudioScale* scale, const Transport* transport,
  const AudioParameterSystem* param_sys, uint32_t pitch_sample_group, AudioBufferHandle bg_buff,
  const AudioBufferHandle* note_buffs, int num_note_buffs) :
  node_id{node_id}, buffer_store{buff_store}, scale{scale},
  transport{transport}, param_sys{param_sys}, pitch_sample_group{pitch_sample_group} {
  //
  if (num_note_buffs == 4) {
    //  First 2 buffers belong set0, second 2 to set1
    for (int i = 0; i < num_note_buffs; i++) {
      note_buff_handles[num_note_buff_handles++] = note_buffs[i];
    }
  } else {
    //  Will just end up with silence in this case if/when asserts are disabled because
    //  `num_note_buff_handles` == 0
    assert(false);
  }

  assert(bg_buff.is_valid());
  bg_buff_handle = bg_buff;

  note_set2(global_semitone_offsets, &num_global_semitone_offsets);

  kb_semitone = note_number_to_semitone(midi_note_number_a4()) + 9.0;
}

InputAudioPorts ChimeSampler::inputs() const {
  InputAudioPorts result{};
//  auto opt_flag = AudioPort::Flags::marked_optional();
  int pi{};
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<ChimeSampler*>(this), pi++});
//  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<ChimeSampler*>(this), pi++, opt_flag});
  return result;
}

OutputAudioPorts ChimeSampler::outputs() const {
  OutputAudioPorts result{};
  int pi{};
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<ChimeSampler*>(this), pi++});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<ChimeSampler*>(this), pi++});
  return result;
}

void ChimeSampler::process(
  const AudioProcessData& in, const AudioProcessData& out,
  AudioEvents*, const AudioRenderInfo& info) {
  //
  if (num_note_buff_handles == 0) {
    return;
  }

  auto maybe_bg_chunk = buffer_store->render_get(bg_buff_handle, 0, 0);
  if (!maybe_bg_chunk || !maybe_bg_chunk.value().descriptor.is_n_channel_float(2)) {
    return;
  }
  const auto& bg_chunk = maybe_bg_chunk.value();

  constexpr int max_num_chunks = 4;
  AudioBufferChunk note_chunks[max_num_chunks];
  for (int i = 0; i < num_note_buff_handles; i++) {
    auto chunk = buffer_store->render_get(note_buff_handles[i], 0, 0);
    if (!chunk || !chunk.value().descriptor.is_n_channel_float(2)) {
      return;
    }
    note_chunks[i] = chunk.value();
  }

  constexpr int global_note_set_param_val = 0;

  { //  params
    uint32_t pi{};
    const auto& changes = param_system::render_read_changes(param_sys);
    auto self_changes = changes.view_by_parent(node_id);
    check_apply_float_param(params.p_quantized, self_changes.view_by_parameter(pi++));
    check_apply_float_param(params.chime_mix, self_changes.view_by_parameter(pi++));
    check_apply_int_param(params.duration_index, self_changes.view_by_parameter(pi++));
    check_apply_int_param(params.buffer_set_index, self_changes.view_by_parameter(pi++));
    auto new_set = check_apply_int_param(
      params.note_set_index, self_changes.view_by_parameter(pi++));
    if (new_set && new_set.value() != global_note_set_param_val) {
      note_set2(global_semitone_offsets, &num_global_semitone_offsets);
    }
  }

  if (params.note_set_index.value == global_note_set_param_val) {
    num_global_semitone_offsets = pss::render_read_semitones(
      pss::get_global_pitch_sampling_system(),
      PitchSampleSetGroupHandle{pitch_sample_group}, 0, global_semitone_offsets, 16);
  }

  Vec2f on_time_lims;
  Vec2f decay_time_lims;
  Vec2f event_time_lims;
  get_duration_indexed_time_limits(
    params.duration_index.value, &on_time_lims, &decay_time_lims, &event_time_lims);

  const audio::Quantization on_quant = get_duration_indexed_quantization(params.duration_index.value);
  const auto time_left_lims = get_duration_indexed_time_left_limits(params.duration_index.value);

  const bool use_buff_set1 = params.buffer_set_index.value == 1;

  const auto sps = float(1.0 / info.sample_rate);
#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
#else
  const auto& tuning = *scale->render_get_tuning();
#endif

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      kb_semitone = note_number_to_semitone(message.note_number()) + 9.0;
    }

    const float p_quantized = params.p_quantized.evaluate();
    const float chime_mix = params.chime_mix.evaluate();

    float accum_s0{};
    float accum_s1{};

    if (uint64_t(bg_frame_index) >= bg_chunk.frame_end()) {
      //  @TODO: Fadeout and back in.
      bg_frame_index = 0.0;
    }
    if (uint64_t(bg_frame_index) < bg_chunk.frame_end()) {
      const float bg_gain = 4.0f;
      auto lerp_info = util::make_linear_interpolation_info(bg_frame_index, bg_chunk.frame_end());
      float s0 = util::tick_interpolated_float(bg_chunk, bg_chunk.channel_descriptor(0), lerp_info);
      float s1 = util::tick_interpolated_float(bg_chunk, bg_chunk.channel_descriptor(1), lerp_info);
      accum_s0 += s0 * bg_gain;
      accum_s1 += s1 * bg_gain;
      bg_frame_index += frame_index_increment(bg_chunk.descriptor.sample_rate, info.sample_rate, 1);
    }

    if (time_left_before_new_event <= 0.0) {
      int num_idle{};
      for (Voice& v : voices) {
        if (v.state == NoteState::Idle) {
          num_idle++;
        }
      }

      constexpr int n_thresh = 4;
      if (num_idle >= n_thresh) {
        //  Start new event.
        int ni{};
        for (Voice& voice : voices) {
          if (voice.state == NoteState::Idle) {
            voice.time_left = float(lerp(urand(), time_left_lims.x, time_left_lims.y));
            voice.state = NoteState::AwaitingOnset;
            voice.quantized = false;
            if (urand() < p_quantized) {
              voice.next_quantization = on_quant;
              voice.quantized = true;
            }
            if (++ni == n_thresh) {
              break;
            }
          }
        }
        assert(ni == n_thresh);
        time_left_before_new_event = lerp(urand(), event_time_lims.x, event_time_lims.y);
      }
    } else {
      time_left_before_new_event = std::max(0.0, time_left_before_new_event - sps);
    }

    for (Voice& v : voices) {
      if (v.state != NoteState::AwaitingOnset) {
        continue;
      }

      bool begin_note;
      if (v.quantized) {
        int off = transport->render_get_pausing_cursor_quantized_event_frame_offset(v.next_quantization);
        begin_note = off == i;
      } else {
        v.time_left = std::max(0.0f, v.time_left - sps);
        begin_note = v.time_left == 0.0f;
      }

      if (begin_note) {
        //  Determine note parameters.
        v.frame_index = 0.0;
        v.semitone = 0.0;
        if (num_global_semitone_offsets > 0) {
          v.semitone = global_semitone_offsets[int(urand() * double(num_global_semitone_offsets))];
        }
        v.semitone += kb_semitone;
        v.buff_index = 0;
        if (num_note_buff_handles > 1 && urand() < 0.25) {
          v.buff_index = 1;
        }
        if (use_buff_set1) {
          v.buff_index += Config::buffer_set1_offset;
        }
        v.gain = lerp(urand(), Config::gain_lims.x, Config::gain_lims.y);
        v.timeout = lerp(urand(), on_time_lims.x, on_time_lims.y);
        v.state = NoteState::On;
        v.timeout_state = NoteState::On;
      }
    }

    for (Voice& v : voices) {
      if (v.state != NoteState::On) {
        continue;
      }

      float decay_gain = 1.0f;
      v.timeout = std::max(0.0f, v.timeout - sps);
      if (v.timeout_state == NoteState::Decaying) {
        decay_gain = std::min(1.0f, v.timeout / std::max(1e-3f, v.decay_time));
      }

      bool elapsed{};
      if (v.timeout == 0.0f) {
        if (v.timeout_state == NoteState::On) {
          v.timeout_state = NoteState::Decaying;
          v.timeout = lerp(urand(), decay_time_lims.x, decay_time_lims.y);
          v.decay_time = v.timeout;
        } else {
          assert(v.timeout_state == NoteState::Decaying);
          elapsed = true;
        }
      }

      assert(v.buff_index < uint8_t(max_num_chunks));
      const auto* chunk = &note_chunks[v.buff_index];
      double& fi = v.frame_index;

      if (uint64_t(fi) >= chunk->frame_end() || elapsed) {
        v.state = NoteState::Idle;

      } else {
        auto interp = util::make_linear_interpolation_info(fi, chunk->frame_end());
#if GROVE_PREFER_AUDIO_SCALE_SYS
        (void) scale;
        const double rm = scale_system::render_get_rate_multiplier_from_semitone(scale_sys, v.semitone, i);
#else
        const double rm = semitone_to_rate_multiplier_equal_temperament(v.semitone, tuning);
#endif
        fi += frame_index_increment(chunk->descriptor.sample_rate, info.sample_rate, rm);

        float s0 = util::tick_interpolated_float(*chunk, chunk->channel_descriptor(0), interp);
        float s1 = util::tick_interpolated_float(*chunk, chunk->channel_descriptor(1), interp);
        accum_s0 += s0 * v.gain * decay_gain * chime_mix;
        accum_s1 += s1 * v.gain * decay_gain * chime_mix;
      }
    }

    out.descriptors[0].write(out.buffer.data, i, &accum_s0);
    out.descriptors[1].write(out.buffer.data, i, &accum_s1);
  }
}

void ChimeSampler::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  Params p{};
  AudioParameterDescriptor* dst = mem.push(Params::num_params);
  uint32_t id{};
  int i{};
  dst[i++] = p.p_quantized.make_descriptor(node_id, id++, p.p_quantized.value, "p_quantized");
  dst[i++] = p.chime_mix.make_descriptor(node_id, id++, p.chime_mix.value, "chime_mix");
  dst[i++] = p.duration_index.make_descriptor(node_id, id++, p.duration_index.value, "duration_index");
  dst[i++] = p.buffer_set_index.make_descriptor(node_id, id++, p.buffer_set_index.value, "buffer_set_index");
  dst[i++] = p.note_set_index.make_descriptor(node_id, id++, p.note_set_index.value, "note_set_index");
}

GROVE_NAMESPACE_END
