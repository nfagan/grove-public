#include "AltReverbNode.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

float sample2_to_01_float(Sample2 s) {
  float sn = std::abs(s.samples[0]);
  return 1.0f - std::exp(-sn * 3.0f);
}

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

AltReverbNode::AltReverbNode(AudioParameterID node_id,
                             const AudioParameterSystem* parameter_system) :
  node_id{node_id},
  parameter_system{parameter_system} {
  //
  fixed_osc_sin_freq = semitone_to_frequency(semitone_a4() + urand_11() * 24.0);
  fixed_osc_env.configure(randomized_params());
}

void AltReverbNode::process(
  const AudioProcessData& in, const AudioProcessData& out, AudioEvents*,
  const AudioRenderInfo& info) {
  //
  if (info.sample_rate != last_sample_rate) {
    last_sample_rate = info.sample_rate;
    reverb.set_sample_rate(info.sample_rate);
  }

  if (fixed_osc_env.elapsed() && grove::urand() > 0.95) {
    fixed_osc_env.configure(randomized_params());
    fixed_osc_env.note_on();
  }

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  const auto self_changes = param_changes.view_by_parent(node_id);
  const auto mix_changes = self_changes.view_by_parameter(0);
  const auto fb_changes = self_changes.view_by_parameter(1);

  const auto fixed_osc_mix_changes = self_changes.view_by_parameter(2);
  AudioParameterChange fixed_osc_mix_change{};
  if (fixed_osc_mix_changes.collapse_to_last_change(&fixed_osc_mix_change)) {
    fixed_osc_mix.apply(fixed_osc_mix_change);
  }

  int mix_change_index{};
  int fb_change_index{};

  constexpr float fb_span = Reverb1::FDNFeedbackLimits::max - Reverb1::FDNFeedbackLimits::min;
  constexpr float fb_min = Reverb1::FDNFeedbackLimits::min;

  Sample2 latest_sample{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(mix_changes, mix_change_index, mix, i);
    maybe_apply_change(fb_changes, fb_change_index, fdn_feedback, i);
    /*
     * @TODO: Determine why the following doesn't (always) work on MSVC:
     * Sample2 src;
     * in.descriptors[0].read(in.buffer.data, i, &src.samples[0]);
     * in.descriptors[1].read(in.buffer.data, i, &src.samples[1]);
     */
    float s0;
    float s1;
    in.descriptors[0].read(in.buffer.data, i, &s0);
    in.descriptors[1].read(in.buffer.data, i, &s1);
    Sample2 sample{{s0, s1}};

    const float feedback_value = fb_min + fdn_feedback.evaluate() * fb_span;
    const float mix_value = mix.evaluate();

    const float fixed_osc_gain = lerp(fixed_osc_env.tick(info.sample_rate), 0.25f, 1.0f);
    const float fixed_osc_mix_t = fixed_osc_mix.evaluate();
    const auto fixed_osc_val = fixed_osc_gain * osc::Sin::tick(
      info.sample_rate, &fixed_osc_sin_phase, fixed_osc_sin_freq);

    sample.samples[0] = lerp(fixed_osc_mix_t, sample.samples[0], float(fixed_osc_val));
    sample.samples[1] = lerp(fixed_osc_mix_t, sample.samples[1], float(fixed_osc_val));
    sample = reverb.tick(sample, info.sample_rate, feedback_value, mix_value);

    latest_sample = sample;
    s0 = sample.samples[0];
    s1 = sample.samples[1];
    out.descriptors[0].write(out.buffer.data, i, &s0);
    out.descriptors[1].write(out.buffer.data, i, &s1);
  }

  if (info.num_frames > 0) {
    auto write_frame = info.num_frames - 1;
    auto evt = make_monitorable_parameter_audio_event(
      {node_id, 3},
      make_float_parameter_value(sample2_to_01_float(latest_sample)),
      write_frame, 0);
    evt.frame = write_frame;
    auto evt_stream = audio_event_system::default_event_stream();
    (void) audio_event_system::render_push_event(evt_stream, evt);
  }
}

void AltReverbNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  const auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();

  const int np = 4;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  dst[i++] = mix.make_descriptor(node_id, p++, 0.0f, "mix");
  dst[i++] = fdn_feedback.make_descriptor(node_id, p++, default_feedback, "feedback");
  dst[i++] = fixed_osc_mix.make_descriptor(node_id, p++, 0.0f, "fixed_osc_mix");
  dst[i++] = signal_representation.make_descriptor(
    node_id, p++, 0.0f, "signal_representation", monitor_flags);
}

InputAudioPorts AltReverbNode::inputs() const {
  InputAudioPorts result;
  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<AltReverbNode*>(this), 0});
  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<AltReverbNode*>(this), 1});
  return result;
}

OutputAudioPorts AltReverbNode::outputs() const {
  OutputAudioPorts result;
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<AltReverbNode*>(this), 0});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<AltReverbNode*>(this), 1});
  return result;
}

GROVE_NAMESPACE_END
