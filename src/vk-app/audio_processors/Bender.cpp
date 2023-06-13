#include "Bender.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double max_delay_time_s() {
  return 0.5;
}

constexpr double center_delay_time_s() {
  return 0.05;
}

constexpr double dflt_lfo_mod_time() {
  return double(Bender::DelayTimeMsLimits::min * 1e-3f);
}

constexpr double dflt_lfo_mod_freq() {
  return 1.0;
}

float collapse_channels(Sample2 sample) {
  return (sample.samples[0] + sample.samples[1]) * 0.5f;
}

float normalize_01(float s) {
  return clamp(std::abs(s), 0.0f, 1.0f);
}

} //  anon

Bender::Bender(AudioParameterID node_id, const Transport* transport, bool emit_events) :
  node_id{node_id},
  transport{transport},
  short_delay{default_sample_rate(),
              max_delay_time_s(),
              center_delay_time_s(),
              dflt_lfo_mod_time(),
              dflt_lfo_mod_freq()},
  emit_events{emit_events} {
  //
  int input_ind{};
  for (int i = 0; i < 2; i++) {
    input_ports.push_back(InputAudioPort{BufferDataType::Float, this, input_ind++});
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  auto opt_flags = AudioPort::Flags::marked_optional();
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, input_ind++, opt_flags});
}

InputAudioPorts Bender::inputs() const {
  return input_ports;
}

OutputAudioPorts Bender::outputs() const {
  return output_ports;
}

void Bender::process(const AudioProcessData& in,
                     const AudioProcessData& out,
                     AudioEvents* events,
                     const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  const auto tsig = reference_time_signature();
  const auto bps = beats_per_sample_at_bpm(transport->get_bpm(), info.sample_rate, tsig);

  if (transport->just_played()) {
    cursor = {};
    last_quantum = -1.0;
  }

  const auto& in0 = in.descriptors[0];
  const auto& in1 = in.descriptors[1];
  const auto& quant_descriptor = in.descriptors[2];

  const auto& out0 = out.descriptors[0];
  const auto& out1 = out.descriptors[1];

  int quant_changed_frame{-1};
  Sample2 signal_rep{};

  for (int i = 0; i < info.num_frames; i++) {
    if (!quant_descriptor.is_missing()) {
      float qv;
      quant_descriptor.read(in.buffer.data, i, &qv);
      if (high_epoch && qv < 0.0f) {
        quantization = audio::Quantization::Eighth;
        high_epoch = !high_epoch;

      } else if (!high_epoch && qv > 0.0f) {
        quantization = audio::Quantization::Sixteenth;
        high_epoch = !high_epoch;
      }
    }

    auto quant = audio::quantize_floor(cursor.beat, quantization, tsig.numerator);
    if (quant != last_quantum) {
      quant_changed_frame = i;

      const auto& lims = delay_time.limits;
      auto delay_val = target_short ? lims.minimum() : lims.maximum();
      auto time_val = (urandf() * (lims.maximum() - lims.minimum()) + lims.minimum()) * 1e-3f;
      target_short = !target_short;
      auto change = make_audio_parameter_change(
        {}, make_float_parameter_value(delay_val), 0, int(info.sample_rate * time_val));
      delay_time.apply(change);
      last_quantum = quant;
    }

    auto dt = delay_time.evaluate() * 1e-3f;
    short_delay.set_center_delay_time(dt);

    float s0;
    float s1;
    in0.read(in.buffer.data, i, &s0);
    in1.read(in.buffer.data, i, &s1);
    Sample2 sample;
    sample.samples[0] = s0;
    sample.samples[1] = s1;

    auto delayed = short_delay.tick(sample, info.sample_rate, 0.0);
    signal_rep = delayed;

    s0 = delayed.samples[0];
    s1 = delayed.samples[1];
    out0.write(out.buffer.data, i, &s0);
    out1.write(out.buffer.data, i, &s1);

    cursor.wrapped_add_beats(bps, tsig.numerator);
  }

  if (emit_events && info.num_frames > 0) {
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
    const auto evt_stream = audio_event_system::default_event_stream();
    (void) events;
#endif
    if (quant_changed_frame != -1) {
      auto quant_val = make_int_parameter_value(1);
      auto quant_evt = make_monitorable_parameter_audio_event(
        {node_id, 0}, quant_val, quant_changed_frame, 0);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
      (void) audio_event_system::render_push_event(evt_stream, quant_evt);
#else
      events[quant_changed_frame].push_back(quant_evt);
#endif
    }

    auto signal_val = make_float_parameter_value(normalize_01(collapse_channels(signal_rep)));
    auto signal_evt = make_monitorable_parameter_audio_event(
      {node_id, 1}, signal_val, info.num_frames-1, 0);
#if GROVE_INCLUDE_NEW_EVENT_SYSTEM
    (void) audio_event_system::render_push_event(evt_stream, signal_evt);
#else
    events[info.num_frames-1].push_back(signal_evt);
#endif
  }
}

void Bender::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  const auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  const int np = 2;
  auto* dst = mem.push(np);
  uint32_t p{};
  int i{};
  dst[i++] = quantization_representation.make_descriptor(
    node_id, p++, 0, "quantization_representation", monitor_flags);
  dst[i++] = signal_representation.make_descriptor(
    node_id, p++, 0.0f, "signal_representation", monitor_flags);
}

GROVE_NAMESPACE_END
