#include "BufferStoreSampler.hpp"
#include "signal.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double semitone_offset() {
  //  @TODO: Incorporate as parameter in AudioBufferStore.
  //  I'm used to dealing with C3-referenced samples, but in this program
  //  we use an A4 reference pitch
  return 9.0;
}

} //  anon

BufferStoreSampler::BufferStoreSampler(
  uint32_t node_id, const AudioBufferStore* buffer_store,
  AudioBufferHandle buffer_handle, const AudioScale* scale, bool enable_events) :
  //
  node_id{node_id},
  buffer_store{buffer_store},
  buffer_handle{buffer_handle},
  scale{scale},
  enable_events{enable_events} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  input_ports.push_back(InputAudioPort{
    BufferDataType::Float, this, 1, AudioPort::Flags::marked_optional()});

  for (int i = 0; i < 2; i++) {
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }

  for (auto& env : envelopes) {
    env.configure(Envelope::Params::default_exp());
  }
  for (auto& rm : rate_multipliers) {
    rm = 1.0;
  }
}

InputAudioPorts BufferStoreSampler::inputs() const {
  return input_ports;
}

OutputAudioPorts BufferStoreSampler::outputs() const {
  return output_ports;
}

void BufferStoreSampler::process(const AudioProcessData& in, const AudioProcessData& out,
                                 AudioEvents*, const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  auto& in0 = in.descriptors[0];
  auto& in1 = in.descriptors[1];

  auto maybe_buffer_chunk = buffer_store->render_get(buffer_handle, 0, info);
  if (!maybe_buffer_chunk) {
    GROVE_LOG_WARNING_CAPTURE_META("Failed to load buffer.", "BufferStoreSampler");
    return;
  }

  auto& chunk = maybe_buffer_chunk.value();
  if (!chunk.descriptor.is_compatible_with(out.descriptors)) {
    GROVE_LOG_WARNING_CAPTURE_META("Buffer is incompatible with output.", "BufferStoreSampler");
    return;
  }

#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
  (void) scale;
#else
  const auto* tuning = scale->render_get_tuning();
#endif
  for (int i = 0; i < num_voices; i++) {
    const double st = note_number_to_semitone(note_numbers[i]) + semitone_offset();
#if GROVE_PREFER_AUDIO_SCALE_SYS
    rate_multipliers[i] = scale_system::render_get_rate_multiplier_from_semitone(
      scale_sys, st, std::max(0, info.num_frames - 1));
#else
    rate_multipliers[i] = semitone_to_rate_multiplier_equal_temperament(
      st, *tuning);
#endif
  }

  for (int i = 0; i < info.num_frames; i++) {
    const uint64_t frame = info.render_frame + i;

    MIDIMessage message{};
    in0.read(in.buffer.data, i, &message);

    float amp_mod{1.0f};
    if (!in1.is_missing()) {
      in1.read(in.buffer.data, i, &amp_mod);
    }

    if (message.is_note_on()) {
      const uint8_t note_number = message.note_number();
      const auto voice_ind = voice_allocator.note_on_reuse_active(frame, note_number);
      const double st = note_number_to_semitone(note_number) + semitone_offset();
      envelopes[voice_ind].note_on();
      note_numbers[voice_ind] = note_number;
#if GROVE_PREFER_AUDIO_SCALE_SYS
      rate_multipliers[voice_ind] = scale_system::render_get_rate_multiplier_from_semitone(
        scale_sys, st, info.num_frames - 1);
#else
      rate_multipliers[voice_ind] = semitone_to_rate_multiplier_equal_temperament(
        st, *tuning);
#endif
      frame_indices[voice_ind] = 0.0;

    } else if (message.is_note_off()) {
      if (auto voice_ind = voice_allocator.note_off(message.note_number())) {
        envelopes[voice_ind.value()].note_off();
      }
    }

    for (int j = 0; j < chunk.descriptor.num_channels(); j++) {
      float zero = 0.0f;
      out.descriptors[j].write(out.buffer.data, i, &zero);
    }

    for (int v = 0; v < num_voices; v++) {
      auto& frame_index = frame_indices[v];
      auto& rate_multiplier = rate_multipliers[v];
      auto& envelope = envelopes[v];

      auto s0 = std::floor(frame_index);
      auto i0 = uint64_t(s0);
      auto i1 = i0 + 1;
      auto f0 = frame_index - s0;

      auto env_val = envelope.tick(info.sample_rate) * amp_mod;

      if (voice_allocator.is_active(v) && envelope.elapsed()) {
        voice_allocator.deallocate(v);
      }

      if (chunk.is_in_bounds(i0) && chunk.is_in_bounds(i1)) {
        for (int j = 0; j < chunk.descriptor.num_channels(); j++) {
          float v0;
          float v1;

          chunk.read(j, i0, &v0);
          chunk.read(j, i1, &v1);

          float sample = lerp(float(f0), v0, v1) * env_val;
          float curr{};
          out.descriptors[j].read(out.buffer.data, i, &curr);
          sample += curr;
          out.descriptors[j].write(out.buffer.data, i, &sample);
        }
      }

      frame_index += frame_index_increment(
        chunk.descriptor.sample_rate, info.sample_rate, rate_multiplier);
    }
  }

  if (enable_events && info.num_frames > 0) {
    float v{};
    if (audio::mean_signal_amplitude<64>(out.buffer, out.descriptors[0], info.num_frames, &v)) {
      const float min_db = -50.0f;
      const float max_db = 12.0f;
      v = (clamp(float(amplitude_to_db(v)), min_db, max_db) - min_db) / (max_db - min_db);

      auto stream = audio_event_system::default_event_stream();
      const int write_frame = info.num_frames - 1;
      auto evt = make_monitorable_parameter_audio_event(
        {node_id, 0}, make_float_parameter_value(v), write_frame, 0);
      (void) audio_event_system::render_push_event(stream, evt);
    }
  }
}

void BufferStoreSampler::parameter_descriptors(
  TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  //
  auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  auto* dst = mem.push(1);
  dst[0] = signal_repr.make_descriptor(node_id, 0, 0.0f, "signal_representation", monitor_flags);
}

GROVE_NAMESPACE_END
