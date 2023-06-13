#include "RandomizedInstrumentNode.hpp"
#include "note_sets.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using PitchClasses = DynamicArray<PitchClass, 8>;
using Octaves = DynamicArray<int8_t, 8>;

void minor_key1(PitchClasses& pitch_classes, Octaves& octaves, int off) {
  notes::minor_key1(pitch_classes, off);
  notes::center_biased_octave_set(octaves);
}

void lydian_e(PitchClasses& pitch_classes, Octaves& octaves, int off) {
  notes::lydian_e(pitch_classes, off);
  notes::center_biased_octave_set(octaves);
}

} //  anon

RandomizedInstrumentNode::RandomizedInstrumentNode(AudioParameterID node_id,
                                                   const AudioParameterSystem* param_sys) :
  node_id{node_id},
  parameter_system{param_sys} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});

  for (int i = 0; i < 2; i++) {
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  oscillator.fill_sin();
  oscillator.normalize();

  Envelope::Params env_params{};
  env_params.attack_time = 4.0;
  env_params.decay_time = 4.0;
  env_params.sustain_time = 0.0;
  env_params.sustain_amp = 0.0;
  env_params.release_time = 0.0;
  env_params.infinite_sustain = false;
  envelope.configure(env_params);

  randomize_frequency();
}

void RandomizedInstrumentNode::process(const AudioProcessData& in,
                                       const AudioProcessData& out,
                                       AudioEvents* events,
                                       const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  oscillator.set_sample_rate(info.sample_rate);

  if (envelope.elapsed() && grove::urand() > 0.95) {
    envelope.note_on();
  }

  const auto param_changes = param_system::render_read_changes(parameter_system).view_by_parent(node_id);

  int waveform_ind = 0;
  int scale_type_ind = 0;

  const auto waveform_view = param_changes.view_by_parameter(0);
  const auto scale_type_view = param_changes.view_by_parameter(1);

  const double amp_factor = 1.0 / double(out.descriptors.size());

  int latest_note_change_frame{-1};

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    assert(in.descriptors[0].is_midi_message());
    in.descriptors[0].read(in.buffer.data, i, &message);

    auto last_scale_type = scale_type.value;
    maybe_apply_change(scale_type_view, scale_type_ind, scale_type, i);
    auto new_scale_type = scale_type.evaluate();

    if (new_scale_type != last_scale_type) {
      randomize_frequency();
    }

    if (message.is_note_on()) {
      auto note = MIDINote::from_note_number(message.note_number());
      key = int(note.pitch_class);
      randomize_frequency();

      latest_note_change_frame = i;
    }

    auto last_waveform_type = waveform_type.value;
    maybe_apply_change(waveform_view, waveform_ind, waveform_type, i);
    auto new_waveform_type = waveform_type.evaluate();

    if (new_waveform_type != last_waveform_type) {
      apply_new_waveform();
    }

    auto osc_val = oscillator.tick();
    auto env_val = envelope.tick(info.sample_rate);

    auto samp = float(osc_val * env_val * amp_factor * db_to_amplitude(-7.0));

    for (auto& descriptor : out.descriptors) {
      assert(descriptor.is_float());
      descriptor.write(out.buffer.data, i, &samp);
    }
  }

  if (info.num_frames > 0) {
    [&]() -> void {
      //  Emit signal representation event.
      int write_frame = info.num_frames - 1;
      const int frame_dist = 0;
      auto sample_val = float(envelope.get_current_amplitude());

      auto param_val = make_float_parameter_value(sample_val);
      auto event = make_monitorable_parameter_audio_event(
        {node_id, 2}, param_val, write_frame, frame_dist);

      events[write_frame].push_back(event);
    }();

    if (latest_note_change_frame >= 0) {
      //  Emit note change event.
      [&]() -> void {
        int write_frame = latest_note_change_frame;
        const int frame_dist = 0;
        auto param_val = make_int_parameter_value(note_number);

        auto event = make_monitorable_parameter_audio_event(
          {node_id, 3}, param_val, write_frame, frame_dist);

        events[write_frame].push_back(event);
      }();
    }
  }
}

void RandomizedInstrumentNode::apply_new_waveform() {
  switch (waveform_type.value) {
    case 0:
      oscillator.fill_sin();
      break;
    case 1:
      oscillator.fill_tri(4);
      break;
    case 2:
      oscillator.fill_square(8);
      break;
    default:
      break;
  }

  oscillator.normalize();
}

void RandomizedInstrumentNode::randomize_frequency() {
  PitchClasses pitch_classes;
  Octaves octaves;

  switch (scale_type.value) {
    case 0:
      minor_key1(pitch_classes, octaves, key);
      break;
    case 1:
      lydian_e(pitch_classes, octaves, key);
      break;
  }

  if (!pitch_classes.empty() && !octaves.empty()) {
    auto pc = pitch_classes[int(urand() * pitch_classes.size())];
    auto oct = octaves[int(urand() * octaves.size())];
    auto note = MIDINote{pc, oct, 127};
    oscillator.set_frequency(note.frequency());
    note_number = note.note_number();
  }
}

OutputAudioPorts RandomizedInstrumentNode::outputs() const {
  return output_ports;
}

InputAudioPorts RandomizedInstrumentNode::inputs() const {
  return input_ports;
}

void RandomizedInstrumentNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  AudioParameterDescriptor::Flags monitorable_flags{};
  monitorable_flags.mark_non_editable();
  monitorable_flags.mark_monitorable();

  const int np = 4;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  dst[i++] = waveform_type.make_descriptor(node_id, p++, 0, "waveform_type");
  dst[i++] = scale_type.make_descriptor(node_id, p++, 0, "scale_type");
  dst[i++] = signal_representation.make_descriptor(
    node_id, p++, 0.0f, "signal_representation", monitorable_flags);
  dst[i++] = note_number_representation.make_descriptor(
    node_id, p++, 0, "note_number_representation", monitorable_flags);
}

GROVE_NAMESPACE_END
