#include "TriggeredEnvelope.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr float trigger_threshold() {
  return 0.75f;
}

} //  anon

audio::TriggeredEnvelope::TriggeredEnvelope() {
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});

  auto params = Envelope::Params::default_exp(false);
  envelope.configure(params);
}

InputAudioPorts audio::TriggeredEnvelope::inputs() const {
  return input_ports;
}

OutputAudioPorts audio::TriggeredEnvelope::outputs() const {
  return output_ports;
}

void audio::TriggeredEnvelope::process(const AudioProcessData& in,
                                       const AudioProcessData& out,
                                       AudioEvents*,
                                       const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  auto& trigger_descriptor = in.descriptors[0];
  auto& env_descriptor = out.descriptors[0];

  for (int i = 0; i < info.num_frames; i++) {
    float cv;
    trigger_descriptor.read(in.buffer.data, i, &cv);

    if (cv > trigger_threshold() && !triggered && envelope.elapsed()) {
      envelope.note_on();
      triggered = true;
    } else if (cv < trigger_threshold() && triggered) {
      triggered = false;
    }

    float env_val = envelope.tick(info.sample_rate);
    env_descriptor.write(out.buffer.data, i, &env_val);
  }
}

GROVE_NAMESPACE_END
