#include "RhythmicDelay1.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double max_delay_time_s() {
  return RhythmicDelay1::DelayTimeLimits::max;
}

float sample2_to_01_float(Sample2 s) {
#if 0
  float sn = std::abs(s.samples[0] + s.samples[1]) * 0.5f;
#else
  float sn = std::abs(s.samples[0]);
#endif
  return 1.0f - std::exp(-sn * 3.0f);
}

} //  anon

RhythmicDelay1::RhythmicDelay1(AudioParameterID node_id,
                               const AudioParameterSystem* parameter_system) :
  node_id{node_id},
  parameter_system{parameter_system},
  sample_rate{default_sample_rate()},
  delay{default_sample_rate(), max_delay_time_s()} {
  //
  for (int i = 0; i < 2; i++) {
    input_ports.push_back(InputAudioPort{BufferDataType::Float, this, i});
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  //  chorus
  const double chorus_delay_times[2] = {0.003, 0.007};
  const double max_delay_time = 0.01;
  const double mod_time = 0.0019;
  for (int i = 0; i < 2; i++) {
    mod_delays[i] = audio::ModulatedDelayLine<float>{
      default_sample_rate(),
      max_delay_time,
      chorus_delay_times[i],
      mod_time,
      1.01,
      float(i) * grove::pi_over_four()
    };
  }

  //  noise
  noise_osc.fill_white_noise();
  noise_osc.set_frequency(0.125);

  noise_amp_lfo.set_sample_rate(sample_rate);
  noise_amp_lfo.set_frequency(0.01);
}

InputAudioPorts RhythmicDelay1::inputs() const {
  return input_ports;
}

OutputAudioPorts RhythmicDelay1::outputs() const {
  return output_ports;
}

void RhythmicDelay1::process(const AudioProcessData& in,
                             const AudioProcessData& out,
                             AudioEvents* events,
                             const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == 2 && out.descriptors.size() == 2);

  if (info.sample_rate != sample_rate) {
    sample_rate = info.sample_rate;
    delay = audio::InterpolatedDelayLine<Sample2>{sample_rate, max_delay_time_s()};
    for (auto& del : mod_delays) {
      del.change_sample_rate(sample_rate);
    }
    noise_amp_lfo.set_sample_rate(sample_rate);
  }

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  const auto self_changes = param_changes.view_by_parent(node_id);
  auto delay_time_changes = self_changes.view_by_parameter(0);
  auto mix_changes = self_changes.view_by_parameter(1);
  auto chorus_mix_changes = self_changes.view_by_parameter(2);
  auto noise_mix_changes = self_changes.view_by_parameter(3);

  int mix_change_index{};
  int chorus_mix_change_index{};
  int noise_mix_change_index{};
  int delay_time_change_index{};

  const auto& in0 = in.descriptors[0];
  const auto& in1 = in.descriptors[1];
  const auto& out0 = out.descriptors[0];
  const auto& out1 = out.descriptors[1];

  Sample2 monitorable_value{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(mix_changes, mix_change_index, mix, i);
    maybe_apply_change(chorus_mix_changes, chorus_mix_change_index, chorus_mix, i);
    maybe_apply_change(noise_mix_changes, noise_mix_change_index, noise_mix, i);
    maybe_apply_change(delay_time_changes, delay_time_change_index, delay_time, i);

    auto mix_val = mix.evaluate();
    auto chorus_mix_val = chorus_mix.evaluate();
    auto noise_mix_val = noise_mix.evaluate();
    auto delay_t = delay_time.evaluate();

    float s0{};
    float s1{};
    Sample2 sample;

    in0.read(in.buffer.data, i, &s0);
    in1.read(in.buffer.data, i, &s1);

    sample.samples[0] = s0;
    sample.samples[1] = s1;

    auto delayed = delay.tick(sample, delay_t, sample_rate);

    //  chorus
    for (int j = 0; j < 2; j++) {
      auto mod_delayed = mod_delays[j].tick(delayed.samples[j], sample_rate, 0.0);
      delayed.samples[j] = lerp(chorus_mix_val, delayed.samples[j], mod_delayed);
    }

    //  noise
    auto noise_amp_val = 0.5f + 0.5f * (noise_amp_lfo.tick() * 0.5f + 0.5f);
    Sample2 noise_val;
    noise_val.assign(noise_osc.tick() * noise_amp_val * noise_gain);
    delayed = lerp(noise_mix_val, delayed, noise_val);

    auto res = lerp(mix_val, sample, delayed);
    monitorable_value = res;

    s0 = res.samples[0];
    s1 = res.samples[1];
    out0.write(out.buffer.data, i, &s0);
    out1.write(out.buffer.data, i, &s1);
  }

  if (info.num_frames > 0) {
    auto write_frame = info.num_frames - 1;
    auto frame_dist = 0;
    auto evt = make_monitorable_parameter_audio_event(
      {node_id, 4},
      make_float_parameter_value(sample2_to_01_float(monitorable_value)),
      write_frame, frame_dist);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
    evt.frame = write_frame;
    auto evt_stream = audio_event_system::default_event_stream();
    (void) audio_event_system::render_push_event(evt_stream, evt);
    (void) events;
#else
    events[write_frame].push_back(evt);
#endif
  }
}

void RhythmicDelay1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  AudioParameterDescriptor::Flags monitorable_flags{};
  monitorable_flags.mark_monitorable();
  monitorable_flags.mark_non_editable();

  const int np = 5;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  dst[i++] = delay_time.make_descriptor(node_id, p++, default_delay_time, "delay_time");
  dst[i++] = mix.make_descriptor(node_id, p++, 0.5f, "mix");
  dst[i++] = chorus_mix.make_descriptor(node_id, p++, 0.5f, "chorus_mix");
  dst[i++] = noise_mix.make_descriptor(node_id, p++, 0.0f, "noise_mix");
  dst[i++] = signal_representation.make_descriptor(
    node_id, p++, 0.0f, "signal_representation", monitorable_flags);
}

GROVE_NAMESPACE_END
