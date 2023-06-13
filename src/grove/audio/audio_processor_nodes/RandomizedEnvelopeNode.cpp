#include "RandomizedEnvelopeNode.hpp"
#include "../AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"
#include "grove/audio/AudioEventSystem.hpp"

GROVE_NAMESPACE_BEGIN

RandomizedEnvelopeNode::RandomizedEnvelopeNode(AudioParameterID node_id,
                                               const AudioParameterSystem* parameter_system,
                                               int num_outputs,
                                               bool emit_events) :
                                               node_id{node_id},
                                               parameter_system{parameter_system},
                                               lfo{default_sample_rate()},
                                               emit_events{emit_events} {
  Envelope::Params env_params{};
  env_params.attack_time = 4.0;
  env_params.decay_time = 4.0;
  env_params.sustain_time = 0.0;
  env_params.sustain_amp = 0.0;
  env_params.release_time = 0.0;
  env_params.infinite_sustain = false;
  envelope.configure(env_params);
  //
  for (int i = 0; i < num_outputs; i++) {
    output_ports.push_back(OutputAudioPort(BufferDataType::Float, this, i));
  }
}

void RandomizedEnvelopeNode::process(const AudioProcessData&,
                                     const AudioProcessData& out,
                                     AudioEvents* events,
                                     const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUT(out, output_ports);

  if (envelope.elapsed() && urand() > 0.95) {
    envelope.note_on();
  }

  lfo.set_sample_rate(info.sample_rate);
  lfo.set_frequency(9.0);

  const auto& changes = param_system::render_read_changes(parameter_system);
  auto self_changes = changes.view_by_parent(node_id);
  auto amp_mod_changes = self_changes.view_by_parameter(0);

  int amp_mod_change_index{};

  float amp{};
  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(amp_mod_changes, amp_mod_change_index, amplitude_modulation_amount, i);
    amp = float(envelope.tick(info.sample_rate));

    auto amp_mod_amt = amplitude_modulation_amount.evaluate();
    auto amp_mod = float(lfo.tick()) * 0.5f + 0.5f;
    auto amp_modded = amp * amp_mod;
    amp = lerp(amp_mod_amt, amp, amp_modded);

    for (auto& descr : out.descriptors) {
      assert(descr.is_float());
      descr.write(out.buffer.data, i, &amp);
    }
  }

  if (emit_events && info.num_frames > 0) {
    const int write_frame = info.num_frames-1;
    auto env_val = make_float_parameter_value(clamp(amp, 0.0f, 1.0f));
    auto env_evt = make_monitorable_parameter_audio_event({node_id, 1}, env_val, write_frame, 0);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
    env_evt.frame = write_frame;
    const auto evt_stream = audio_event_system::default_event_stream();
    (void) audio_event_system::render_push_event(evt_stream, env_evt);
    (void) events;
#else
    events[write_frame].push_back(env_evt);
#endif
  }
}

InputAudioPorts RandomizedEnvelopeNode::inputs() const {
  return {};
}

OutputAudioPorts RandomizedEnvelopeNode::outputs() const {
  return output_ports;
}

void RandomizedEnvelopeNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  const auto flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  const int np = 2;
  auto* dst = mem.push(np);
  uint32_t p{};
  int i{};
  dst[i++] = amplitude_modulation_amount.make_descriptor(
    node_id, p++, 0.0f, "amplitude_modulation_amount");
  dst[i++] = envelope_representation.make_descriptor(
    node_id, p++, 0.0f, "envelope_representation", flags);
}

GROVE_NAMESPACE_END
