#include "ArpNode.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/audio/cursor.hpp"
#include "grove/audio/AudioParameterSystem.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

double rate_index_to_rate_multiplier(int v) {
  switch (v) {
    case 0:
      return 1.0;
    case 1:
      return 2.0;
    case 2:
      return 4.0;
    default:
      assert(false);
      return 1.0;
  }
}

uint8_t semitone_step_index_to_semitone_step(int v) {
  switch (v) {
    case 0:
      return 5;
    case 1:
      return 7;
    case 2:
      return 12;
    default:
      assert(false);
      return 0;
  }
}

} //  anon

ArpNode::ArpNode(uint32_t node_id, const Transport* transport,
                 const AudioParameterSystem* param_sys) :
  node_id{node_id}, transport{transport}, parameter_system{param_sys} {
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::MIDIMessage, this, 0});
}

void ArpNode::process(const AudioProcessData& in, const AudioProcessData& out, AudioEvents*,
                      const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  auto& in_desc = in.descriptors[0];
  auto& out_desc = out.descriptors[0];

  auto cursor = transport->render_get_cursor_location();
  if (!transport->render_is_playing()) {
    cursor = transport_stopped_cursor;
  }

  const double num = reference_time_signature().numerator;
  const double bps = beats_per_sample_at_bpm(
    transport->get_bpm(), info.sample_rate, reference_time_signature());

  const auto& all_changes = param_system::render_read_changes(parameter_system);
  auto self_changes = all_changes.view_by_parent(node_id);
  auto st_changes = self_changes.view_by_parameter(0);
  auto rate_changes = self_changes.view_by_parameter(1);

  AudioParameterChange st_change{};
  if (st_changes.collapse_to_last_change(&st_change)) {
    params.semitone_step.apply(st_change);
  }

  AudioParameterChange rate_change{};
  if (rate_changes.collapse_to_last_change(&rate_change)) {
    params.rate.apply(rate_change);
  }

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage in_msg;
    in_desc.read(in.buffer.data, i, &in_msg);

    if (in_msg.is_note_on()) {
      const uint8_t note_num = in_msg.note_number();
      auto* end = possible_note_numbers + num_possible_notes;
      auto it = std::find(possible_note_numbers, end, note_num);
      if (it == end) {
        if (num_possible_notes == max_num_possible_notes) {
          std::rotate(possible_note_numbers, possible_note_numbers + 1, end);
          possible_note_numbers[num_possible_notes-1] = note_num;
        } else {
          possible_note_numbers[num_possible_notes++] = note_num;
        }
      }
    } else if (in_msg.is_note_off()) {
      const uint8_t note_num = in_msg.note_number();
      auto* end = possible_note_numbers + num_possible_notes;
      auto it = std::find(possible_note_numbers, end, note_num);
      if (it != end) {
        if (note_index >= int(it - possible_note_numbers)) {
          note_index = std::max(0, note_index - 1);
        }
        std::rotate(it, it + 1, end);
        num_possible_notes--;
      }
    }

    const uint8_t st_step = semitone_step_index_to_semitone_step(params.semitone_step.evaluate());
    const double rate_mul = rate_index_to_rate_multiplier(params.rate.evaluate());
    const auto div = int32_t(std::floor(cursor.beat * rate_mul));
    if (div != last_division) {
      last_division = div;
      if (playing_note) {
        auto msg = MIDIMessage::make_note_off(0, playing_note.value(), 0);
        assert(message_queue_size < message_queue_capacity);
        if (message_queue_size < message_queue_capacity) {
          message_queue[message_queue_size++] = msg;
        }
        playing_note = NullOpt{};
      }

      if (num_possible_notes > 0) {
        assert(note_index < num_possible_notes);
        const int note_ind = note_index++;
        note_index %= num_possible_notes;

        const uint8_t base_note_num = possible_note_numbers[note_ind];
        const uint8_t st_off = step++ * st_step;
        step %= 3;
        const uint8_t note_num = base_note_num + st_off;

        auto msg = MIDIMessage::make_note_on(0, note_num, 127);
        assert(message_queue_size < message_queue_capacity);
        if (message_queue_size < message_queue_capacity) {
          message_queue[message_queue_size++] = msg;
        }
      }
    }

    MIDIMessage out_msg{};
    if (message_queue_size > 0) {
      out_msg = message_queue[0];
      std::rotate(message_queue, message_queue + 1, message_queue + message_queue_size--);
      if (out_msg.is_note_on()) {
        assert(!playing_note);
        playing_note = out_msg.note_number();
      }
    }

    out_desc.write(out.buffer.data, i, &out_msg);
    cursor.wrapped_add_beats(bps, num);
  }

  transport_stopped_cursor = cursor;
}

void ArpNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& view) const {
  Params ps{};
  auto* dst = view.push(2);
  int i{};
  uint32_t p{};
  dst[i++] = ps.semitone_step.make_descriptor(
    node_id, p++, ps.semitone_step.value, "semitone_step");
  dst[i++] = ps.rate.make_descriptor(node_id, p++, ps.rate.value, "rate");
}

InputAudioPorts ArpNode::inputs() const {
  return input_ports;
}

OutputAudioPorts ArpNode::outputs() const {
  return output_ports;
}

GROVE_NAMESPACE_END
