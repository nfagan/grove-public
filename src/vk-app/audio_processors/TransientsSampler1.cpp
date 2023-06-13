#include "TransientsSampler1.hpp"
#include "parameter.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

audio::Quantization int_to_quant(int v) {
  audio::Quantization quants[3]{
    audio::Quantization::ThirtySecond,
    audio::Quantization::Sixteenth,
    audio::Quantization::Eighth,
  };
  return quants[clamp(v, 0, 2)];
}

} //  anon

TransientsSampler1::TransientsSampler1(
  uint32_t node_id, const Transport* transport, const AudioBufferStore* buff_store,
  AudioBufferHandle buff_handle, const uint32_t* transient_onsets, int num_onsets) :
  node_id{node_id}, transport{transport}, buff_store{buff_store}, buff_handle{buff_handle} {
  //
  for (int i = 0; i < std::min(num_onsets, 32); i++) {
    onsets[this->num_onsets++] = transient_onsets[i];
  }
}

void TransientsSampler1::process(
  const AudioProcessData& in, const AudioProcessData& out, AudioEvents*, const AudioRenderInfo& info) {
  //
  if (num_onsets == 0) {
    return;
  }

  auto maybe_chunk = buff_store->render_get(buff_handle, 0, 0);
  if (!maybe_chunk || !maybe_chunk.value().descriptor.is_n_channel_float(2)) {
    return;
  }

  const auto& chunk = maybe_chunk.value();
  for (int i = 0; i < num_onsets; i++) {
    if (!chunk.is_in_bounds(onsets[i])) {
      assert(false);
      return;
    }
  }

  {
    const auto* param_sys = param_system::get_global_audio_parameter_system();
    const auto& changes = param_system::render_read_changes(param_sys);
    auto self_changes = changes.view_by_parent(node_id);
    uint32_t pi{};
    (void) check_immediate_apply_float_param(params.p_local_quantized, self_changes.view_by_parameter(pi++));
    (void) check_immediate_apply_float_param(params.p_durations_fan_out, self_changes.view_by_parameter(pi++));
    (void) check_immediate_apply_float_param(params.p_global_timeout, self_changes.view_by_parameter(pi++));
    (void) check_apply_int_param(params.local_quantization, self_changes.view_by_parameter(pi++));
    (void) check_immediate_apply_float_param(params.local_time, self_changes.view_by_parameter(pi++));
  }

  const double inv_fs = 1.0 / info.sample_rate;
  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage msg;
    in.descriptors[0].read(in.buffer.data, i, &msg);
    const bool trigger = msg.is_note_on();
    (void) trigger;

    if (local_elapsed) {
      if (global_timeout_state == 0) {
        bool time_out = inter_timeout_time == 0.0 && urand() < params.p_global_timeout.value;
        global_timeout_state = time_out ? 1 : 0;
        global_timeout_time = 1.0;
        if (time_out) {
          inter_timeout_time = 8.0;
        }
      }
      if (global_timeout_state == 1) {  //  timed out
        global_timeout_elapsed_time += inv_fs;
        if (global_timeout_elapsed_time >= global_timeout_time) {
          global_timeout_elapsed_time = 0.0;
          global_timeout_state = 0;
        }
      } else {
        buff_fi = *uniform_array_sample(onsets, num_onsets);
        local_quant = NullOpt{};
        time_left = 0.0;
        right_on = !right_on;
        if (local_duration_state == 0) {
          if (fan_out_timeout == 0.0 && urand() < params.p_durations_fan_out.value) {
            local_duration_state = 2;
          }
        }
        if (local_duration_state == 2) {  //  start fanning out
          local_duration_state = 1;
          local_duration_index = 0;
        }
        if (local_duration_state == 1) {  //  fanning out
          float dur_t = float(local_duration_index++) / 12.0f;
          time_left = lerp(dur_t * dur_t, 50e-3, 500e-3);
          if (dur_t == 1.0f) {
            local_duration_state = 0;
            fan_out_timeout = 24.0;
          }
        } else {
          if (urand() < params.p_local_quantized.value) {
            local_quant = int_to_quant(params.local_quantization.value);
          } else {
            time_left = lerp(urand(), 50e-3, 500e-3);
          }
        }
        local_elapsed = false;
        local_elapsed_time = 0.0;
      }
    } else {
      if (local_quant) {
        local_elapsed = i == transport->render_get_pausing_cursor_quantized_event_frame_offset(local_quant.value());
      } else {
        time_left = std::max(0.0, time_left - inv_fs);
        local_elapsed = time_left == 0.0;
      }
    }

    fan_out_timeout = std::max(0.0, fan_out_timeout - inv_fs);
    inter_timeout_time = std::max(0.0, inter_timeout_time - inv_fs);

    if (uint64_t(buff_fi) >= chunk.frame_end()) {
      buff_fi = 0.0;
      assert(!chunk.empty());
    }

    const float g = 1.0f;
    auto lerp_info = util::make_linear_interpolation_info(buff_fi, chunk.frame_end());
    float s0 = util::tick_interpolated_float(chunk, chunk.channel_descriptor(0), lerp_info);
    float s1 = util::tick_interpolated_float(chunk, chunk.channel_descriptor(1), lerp_info);

    float lr_mask[2]{};
    lr_mask[int(right_on)] = 1.0f;

    const auto tg = float(global_timeout_state == 0);

    s0 *= tg * g * lr_mask[0];
    s1 *= tg * g * lr_mask[1];

    out.descriptors[0].write(out.buffer.data, i, &s0);
    out.descriptors[1].write(out.buffer.data, i, &s1);

    double local_time = lerp(params.local_time.value, 10e-3, 1.0);
    local_elapsed_time = std::min(local_elapsed_time + inv_fs, local_time);
    double fi_mask = local_elapsed_time == local_time ? 0.0 : 1.0;

    buff_fi += fi_mask * frame_index_increment(chunk.descriptor.sample_rate, info.sample_rate, 1.0);
  }
}

InputAudioPorts TransientsSampler1::inputs() const {
  InputAudioPorts result;
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<TransientsSampler1*>(this), 0});
  return result;
}

OutputAudioPorts TransientsSampler1::outputs() const {
  OutputAudioPorts result;
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<TransientsSampler1*>(this), 0});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<TransientsSampler1*>(this), 1});
  return result;
}

void TransientsSampler1::parameter_descriptors(
  TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  //
  Params ps;
  int di{};
  uint32_t pi{};
  auto* dst = mem.push(Params::num_params);
  dst[di++] = ps.p_local_quantized.make_default_descriptor(node_id, pi++, "p_local_quantized");
  dst[di++] = ps.p_durations_fan_out.make_default_descriptor(node_id, pi++, "p_durations_fan_out");
  dst[di++] = ps.p_global_timeout.make_default_descriptor(node_id, pi++, "p_global_timeout");
  dst[di++] = ps.local_quantization.make_default_descriptor(node_id, pi++, "local_quantization");
  dst[di++] = ps.local_time.make_default_descriptor(node_id, pi++, "local_time");
}

GROVE_NAMESPACE_END
