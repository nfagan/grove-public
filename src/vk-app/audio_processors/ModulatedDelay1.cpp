#include "ModulatedDelay1.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

ModulatedDelay1::ModulatedDelay1(AudioParameterID node_id,
                                 const AudioParameterSystem* param_sys, bool emit_events) :
  node_id{node_id},
  parameter_system{param_sys},
  emit_events{emit_events} {
  //
  for (int i = 0; i < 2; i++) {
    input_ports.push_back(InputAudioPort{BufferDataType::Float, this, i});
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  const double chorus_delay_times[num_channels] = {0.003, 0.007};
  const double max_delay_time = 0.1;
  const double mod_time = 0.0019;

  for (int i = 0; i < num_channels; i++) {
    mod_delays[i] = audio::ModulatedDelayLine<float>{
      default_sample_rate(),
      max_delay_time,
      chorus_delay_times[i],
      mod_time,
      1.01,
      float(i) * grove::pi_over_four()
    };

    rhythmic_delays[i] = audio::InterpolatedDelayLine<float>{
      default_sample_rate(),
      0.3
    };
  }
}

void ModulatedDelay1::process(const AudioProcessData& in,
                              const AudioProcessData& out,
                              AudioEvents* events,
                              const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == out.descriptors.size());

  const auto& all_changes = param_system::render_read_changes(parameter_system);
  const auto changes = all_changes.view_by_parent(node_id);
  const auto lfo_freq_changes = changes.view_by_parameter(0);

  int lfo_freq_index{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(lfo_freq_changes, lfo_freq_index, lfo_frequency, i);
    auto lfo_freq = lfo_frequency.evaluate();

    for (auto& delay : mod_delays) {
      delay.set_lfo_frequency(lfo_freq);
    }

    for (int j = 0; j < in.descriptors.size(); j++) {
      float src;
      in.descriptors[j].read(in.buffer.data, i, &src);

      float samp = src;
      if (j < num_channels) {
        samp = lerp(0.5f, samp, mod_delays[j].tick(samp, info.sample_rate, 0.0));
        samp = lerp(0.5f, samp, rhythmic_delays[j].tick(samp, 0.2, info.sample_rate));
      }

      out.descriptors[j].write(out.buffer.data, i, &samp);
    }
  }

  if (emit_events && info.num_frames > 0 && num_channels > 0) {
    int write_frame = info.num_frames - 1;
    const int frame_dist = 0;
    auto lfo_val = float(mod_delays[0].get_current_lfo_value()) * 0.5f + 0.5f;

    auto param_val = make_float_parameter_value(lfo_val);
    auto event = make_monitorable_parameter_audio_event(
      {node_id, 1}, param_val, write_frame, frame_dist);

    events[write_frame].push_back(event);
  }
}

InputAudioPorts ModulatedDelay1::inputs() const {
  return input_ports;
}

OutputAudioPorts ModulatedDelay1::outputs() const {
  return output_ports;
}

void ModulatedDelay1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  AudioParameterDescriptor::Flags repr_flags{};
  repr_flags.mark_monitorable();
  repr_flags.mark_non_editable();

  const int np = 2;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  dst[i++] = lfo_frequency.make_descriptor(node_id, p++, 1.01f, "lfo_frequency");
  dst[i++] = lfo_representation.make_descriptor(node_id, p++, 0.0f, "lfo_representation", repr_flags);
}

GROVE_NAMESPACE_END