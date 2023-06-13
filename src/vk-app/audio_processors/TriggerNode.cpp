#include "TriggerNode.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"

#define USE_MIDI_MESSAGE_INPUT (0)

GROVE_NAMESPACE_BEGIN

audio::TriggerNode::TriggerNode(AudioParameterID node_id,
                                const AudioParameterSystem* parameter_system,
                                float low, float high, int pulse_duration_samples) :
  node_id{node_id},
  parameter_system{parameter_system},
  low{low},
  high{high},
  pulse_duration_samples{pulse_duration_samples} {
  //
#if USE_MIDI_MESSAGE_INPUT
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
#endif
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
}

InputAudioPorts audio::TriggerNode::inputs() const {
  return input_ports;
}

OutputAudioPorts audio::TriggerNode::outputs() const {
  return output_ports;
}

void audio::TriggerNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto* dst = mem.push(1);
  dst[0] = trigger.make_descriptor(node_id, 0, 0, "trigger");
}

void audio::TriggerNode::process(const AudioProcessData& in,
                                 const AudioProcessData& out,
                                 AudioEvents*,
                                 const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  (void) in;

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  auto self_changes = param_changes.view_by_parent(node_id);
  auto trigger_changes = self_changes.view_by_parameter(0);
  int trigger_change_index{};

#if USE_MIDI_MESSAGE_INPUT
  const auto& message_descriptor = in.descriptors[0];
#endif
  const auto& trigger_descriptor = out.descriptors[0];
  for (int i = 0; i < info.num_frames; i++) {
#if USE_MIDI_MESSAGE_INPUT
    MIDIMessage message{};
    message_descriptor.read(in.buffer.data, i, &message);
    if (message.is_note_on() && pulse_index == 0) {
      pulse_index = pulse_duration_samples;
    }
#else
    if (trigger_changes.should_change_now(trigger_change_index, i)) {
      trigger_change_index++;
      if (pulse_index == 0) {
        //  Begin pulse.
        pulse_index = pulse_duration_samples;
      }
    }
#endif

    float value = low;
    if (pulse_index > 0) {
      pulse_index--;
      value = high;
    }

    trigger_descriptor.write(out.buffer.data, i, &value);
  }
}

GROVE_NAMESPACE_END
