#include "RandomizedSynth.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/random.hpp"

#define WRITE_FROM_SAMPLES (0)
#define USE_OSCILLATOR (1)

GROVE_NAMESPACE_BEGIN

namespace {

#if GROVE_LOGGING_ENABLED
constexpr const char* logging_id() {
  return "RandomizedSynth";
}
#endif

constexpr int reverb_param_offset() {
  return 3;
}

} //  anon

RandomizedSynth::RandomizedSynth(AudioParameterID node_id,
                                 const AudioParameterSystem* parameter_system,
                                 const AudioBufferStore* buffer_store,
                                 AudioBufferHandle buffer_handle,
                                 const Params& params) :
  node_id{node_id},
  parameter_system{parameter_system},
  buffer_store{buffer_store},
  buffer_handle{buffer_handle},
  params{params},
  use_oscillator{int(params.use_oscillator)},
  reverb{node_id, reverb_param_offset(), parameter_system, Reverb1Node::Layout::TwoChannelFloat, {}} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});

  for (int i = 0; i < 2; i++) {
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  Envelope::Params env_params{};
  env_params.attack_time = 4.0;
  env_params.decay_time = 4.0;
  env_params.sustain_time = 0.0;
  env_params.sustain_amp = 0.0;
  env_params.release_time = 0.0;
  env_params.infinite_sustain = false;
  envelope.configure(env_params);

  oscillator.fill_tri(4);
//  oscillator.fill_square(4);
  oscillator.normalize();
  oscillator.set_frequency(current_note.frequency());

  reverb.render_set_mix(params.reverb_mix_fraction);
  reverb.render_set_feedback_from_fraction(params.reverb_feedback_fraction);

#if USE_OSCILLATOR

#endif
}

InputAudioPorts RandomizedSynth::inputs() const {
  return input_ports;
}

OutputAudioPorts RandomizedSynth::outputs() const {
  return output_ports;
}

void RandomizedSynth::process(const AudioProcessData& in,
                              const AudioProcessData& out,
                              AudioEvents* events,
                              const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(in.descriptors.size() == 1 && in.descriptors[0].is_midi_message());
  assert(out.descriptors.size() == 2);

  oscillator.set_sample_rate(info.sample_rate);

  if (envelope.elapsed() && urand() > 0.95) {
    envelope.note_on();
  }

  //  Note set pass.
  auto note_ins = note_set.match_process_data_to_inputs<2>(in);
  auto note_outs = note_set.match_process_data_to_outputs<2>(in);

  if (note_ins && note_outs) {
    note_set.process(note_ins.value(), note_outs.value(), events, info);
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Incompatible port layouts for note set.", logging_id());
  }

  //  Maybe randomize the note.
  if (render_should_randomize_note()) {
    current_note = note_set.render_get_randomized_note();
    oscillator.set_frequency(current_note.frequency());
  }

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  auto self_changes = param_changes.view_by_parent(node_id);

  auto type_changes = self_changes.view_by_parameter(0);
  AudioParameterChange last_type{};
  if (type_changes.collapse_to_last_change(&last_type)) {
    use_oscillator.apply(last_type);
  }

  int latest_frame_new_note{-1};
  int latest_new_note_number{};

  //  Signal source pass.
  if (use_oscillator.value) {
    global_gain = float(db_to_amplitude(-10.0));

    for (int i = 0; i < info.num_frames; i++) {
      MIDIMessage message{};
      in.descriptors[0].read(in.buffer.data, i, &message);

      if (message.is_note_on()) {
        current_note = MIDINote::from_note_number(message.note_number());
        oscillator.set_frequency(message.frequency());
        latest_frame_new_note = i;
        latest_new_note_number = message.note_number();
      }

      (void) use_oscillator.evaluate();

      auto osc_val = float(oscillator.tick());
      auto env_val = envelope.tick(info.sample_rate);
      auto note_val = osc_val * env_val * global_gain;

      for (auto& descr : out.descriptors) {
        assert(descr.is_float());
        descr.write(out.buffer.data, i, &note_val);
      }
    }
  } else {
    //  granulator.
    global_gain = 0.5f;

    auto maybe_chunk = buffer_store->render_get(
      buffer_handle, granulator.get_frame_index(), info);

    if (maybe_chunk &&
        maybe_chunk.value().descriptor.is_n_channel_float(2) &&
        maybe_chunk.value().is_complete()) {
      auto& chunk = maybe_chunk.value();

      for (int i = 0; i < info.num_frames; i++) {
        MIDIMessage message{};
        in.descriptors[0].read(in.buffer.data, i, &message);

        if (message.is_note_on()) {
          latest_frame_new_note = i;
          latest_new_note_number = message.note_number();
          current_note = MIDINote::from_note_number(message.note_number());
        }

        (void) use_oscillator.evaluate();

        Granulator::Params gran_params{};
        gran_params.rate_multiplier = semitone_to_rate_multiplier(current_note.semitone());

        auto sample = granulator.tick_sample2(
          chunk.data, chunk.descriptor, info.sample_rate, gran_params);
        auto env_val = envelope.tick(info.sample_rate);
        sample = sample * env_val * global_gain;

        for (int j = 0; j < 2; j++) {
#if WRITE_FROM_SAMPLES
          out.descriptors[j].write(out.buffer.data, i, &sample.samples[j]);
#else
          float v = sample.samples[j];
          out.descriptors[j].write(out.buffer.data, i, &v);
#endif
        }
      }
    }
  }

  //  Reverb pass.
  auto reverb_ins = reverb.match_process_data_to_inputs<2>(out);
  auto reverb_outs = reverb.match_process_data_to_outputs<2>(out);

  if (reverb_ins && reverb_outs) {
    reverb.process(reverb_ins.value(), reverb_outs.value(), events, info);
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Incompatible port layouts for reverb.", logging_id());
  }

  if (params.emit_events && info.num_frames > 0) {
    {
      //  Current envelope value
      const int write_frame = info.num_frames - 1;
      auto curr_value = make_float_parameter_value(envelope.get_current_amplitude());
      events[write_frame].push_back(
        make_monitorable_parameter_audio_event({node_id, 1}, curr_value, write_frame, 0));
    }

    if (latest_frame_new_note >= 0) {
      //  New note
      const int write_frame = latest_frame_new_note;
      auto val = make_int_parameter_value(latest_new_note_number);
      events[write_frame].push_back(
        make_monitorable_parameter_audio_event({node_id, 2}, val, write_frame, 0));
    }
  }
}

void RandomizedSynth::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  AudioParameterDescriptor::Flags monitorable_flags{};
  monitorable_flags.mark_non_editable();
  monitorable_flags.mark_monitorable();

  const int np = 3;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  dst[i++] = use_oscillator.make_descriptor(
    node_id, p++, int(params.use_oscillator), "use_oscillator");
  dst[i++] = envelope_representation.make_descriptor(
    node_id, p++, 0.0f, "envelope_representation", monitorable_flags);
  dst[i++] = new_note_number_representation.make_descriptor(
    node_id, p++, 0, "new_note_number_representation", monitorable_flags);

  //  Reverb params.
  reverb.parameter_descriptors(mem);
}

void RandomizedSynth::ui_randomize_note() {
  should_randomize_note.store(true);
}

bool RandomizedSynth::render_should_randomize_note() {
  bool expect{true};
  return should_randomize_note.compare_exchange_strong(expect, false);
}

GROVE_NAMESPACE_END
