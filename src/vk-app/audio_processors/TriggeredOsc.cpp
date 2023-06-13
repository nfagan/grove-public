#include "TriggeredOsc.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

#define USE_ENV_INPUT (1)

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

} //  anon

TriggeredOsc::TriggeredOsc(AudioParameterID node_id, const AudioScale* scale,
                           const AudioParameterSystem* param_sys) :
  node_id{node_id}, scale{scale}, param_sys{param_sys} {
  for (int i = 0; i < 2; i++) {
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }
#if USE_ENV_INPUT
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 1});
#endif

  env.configure(randomized_params());
}

void TriggeredOsc::process(const AudioProcessData& in, const AudioProcessData& out,
                           AudioEvents*, const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  osc.set_sample_rate(info.sample_rate);
  amp_mod.set_sample_rate(info.sample_rate);

  const auto& changes = param_system::render_read_changes(param_sys);
  auto self_changes = changes.view_by_parent(node_id);

  auto amp_mod_depth_changes = self_changes.view_by_parameter(0);
  auto amp_mod_freq_changes = self_changes.view_by_parameter(1);
  auto semitone_offset_changes = self_changes.view_by_parameter(2);

  int amp_mod_depth_ind{};
  int amp_mod_freq_ind{};
  int semitone_offset_ind{};

#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
  (void) scale;
#else
  const auto& tuning = *scale->render_get_tuning();
#endif
  float last_signal_val{};

  if (env.elapsed() && grove::urand() > 0.95) {
    env.configure(randomized_params());
    env.note_on();
  }

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(amp_mod_depth_changes, amp_mod_depth_ind, params.amp_mod_depth, i);
    maybe_apply_change(amp_mod_freq_changes, amp_mod_freq_ind, params.amp_mod_freq, i);
    maybe_apply_change(semitone_offset_changes, semitone_offset_ind, params.semitone_offset, i);

    const int st_offset = params.semitone_offset.evaluate();

    amp_mod.set_frequency(params.amp_mod_freq.evaluate());
    const float amp_mod_val = lerp(
      params.amp_mod_depth.evaluate(), 1.0f, float(amp_mod.tick() * 0.5 + 0.5));

    MIDIMessage message;
    in.descriptors[0].read(in.buffer.data, i, &message);

    if (message.is_note_on()) {
      current_note_number = message.note_number();
    }

#if USE_ENV_INPUT
    float env_val;
    in.descriptors[1].read(in.buffer.data, i, &env_val);
#else
    const float env_val = env.tick(info.sample_rate);
#endif

#if GROVE_PREFER_AUDIO_SCALE_SYS
    double freq = scale_system::render_get_frequency(
      scale_sys, uint8_t(clamp(current_note_number + st_offset, 0, 255)), i);
#else
    double freq = note_number_to_frequency_equal_temperament(
      uint8_t(clamp(current_note_number + st_offset, 0, 255)), tuning);
#endif
    osc.set_frequency(freq);

    auto sample = float(osc.tick()) * env_val * amp_mod_val;
    last_signal_val = sample;

    for (auto& desc : out.descriptors) {
      assert(desc.is_float());
      desc.write(out.buffer.data, i, &sample);
    }
  }

  if (info.num_frames > 0) {
    auto stream = audio_event_system::default_event_stream();
    auto note_evt = make_monitorable_parameter_audio_event(
      {node_id, 3},
      make_int_parameter_value(int(current_note_number) + params.semitone_offset.value),
      info.num_frames - 1, 0);
    (void) audio_event_system::render_push_event(stream, note_evt);

    auto signal_evt = make_monitorable_parameter_audio_event(
      {node_id, 4},
      make_float_parameter_value(clamp01(std::abs(last_signal_val))),
      info.num_frames - 1, 0);
    (void) audio_event_system::render_push_event(stream, signal_evt);
  }
}

InputAudioPorts TriggeredOsc::inputs() const {
  return input_ports;
}

OutputAudioPorts TriggeredOsc::outputs() const {
  return output_ports;
}

void TriggeredOsc::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();

  Params dst_params{};
  auto* dst = mem.push(5);
  uint32_t p{};
  int i{};
  dst[i++] = dst_params.amp_mod_depth.make_descriptor(
    node_id, p++, dst_params.amp_mod_depth.value, "amp_mod_depth");
  dst[i++] = dst_params.amp_mod_freq.make_descriptor(
    node_id, p++, dst_params.amp_mod_freq.value, "amp_mod_freq");
  dst[i++] = dst_params.semitone_offset.make_descriptor(
    node_id, p++, dst_params.semitone_offset.value, "semitone_offset");

  dst[i++] = dst_params.monitor_note_number.make_descriptor(
    node_id, p++, dst_params.monitor_note_number.value, "monitor_note_number", monitor_flags);
  dst[i++] = dst_params.signal_representation.make_descriptor(
    node_id, p++, dst_params.signal_representation.value, "signal_representation", monitor_flags);
}

GROVE_NAMESPACE_END
