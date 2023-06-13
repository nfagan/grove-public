#include "ModulatedSampler.hpp"
#include "grove/audio/AudioBufferStore.hpp"
#include "grove/audio/Transport.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

ModulatedSampler::ModulatedSampler(AudioParameterID node_id,
                                   const AudioBufferStore* buffer_store,
                                   AudioBufferHandle buffer_handle,
                                   const AudioParameterSystem* parameter_system,
                                   const Transport* transport) :
  node_id{node_id},
  buffer_store{buffer_store},
  buffer_handle{buffer_handle},
  transport{transport},
  reverb{node_id, 0, parameter_system, Reverb1Node::Layout::Sample2, {}} {
  //
  input_ports.push_back(InputAudioPort{BufferDataType::MIDIMessage, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Sample2, this, 0});

  envelope.configure(Envelope::Params::default_exp());

  reverb.fdn_feedback.value = reverb.fdn_feedback.clamp(0.94f);
  reverb.mix.value = 0.25f;
}

void ModulatedSampler::process(const AudioProcessData& in,
                               const AudioProcessData& out,
                               AudioEvents* events,
                               const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);
  assert(out.descriptors.size() == 1 && out.descriptors[0].is_sample2());
  assert(in.descriptors.size() == 1 && in.descriptors[0].is_midi_message());

  auto& in0 = in.descriptors[0];
  auto& out0 = out.descriptors[0];

  if (last_render_frame != info.render_frame) {
    notes_on = 0;
  }

  rate_multiplier_lfo.set_sample_rate(info.sample_rate);

  auto spb = 1.0 / beats_per_sample_at_bpm(
    transport->get_bpm(), info.sample_rate, reference_time_signature());
  auto delay_time = 0.5 * spb / info.sample_rate;

  auto maybe_chunk = buffer_store->render_get(buffer_handle, frame_index, info);
  if (maybe_chunk && maybe_chunk.value().descriptor.is_n_channel_float(2)) {
    auto& chunk = maybe_chunk.value();

    const auto num_frames = chunk.num_frames_in_source();
    const auto src0 = chunk.descriptor.layout.channel_descriptor(0);
    const auto src1 = chunk.descriptor.layout.channel_descriptor(1);

    for (int i = 0; i < info.num_frames; i++) {
      MIDIMessage message{};
      in0.read(in.buffer.data, i, &message);

      if (message.is_note_on()) {
        envelope.note_on();
        center_rate_multiplier = semitone_to_rate_multiplier(message.semitone());
        frame_index = 0.0;
        notes_on++;

      } else if (message.is_note_off()) {
        notes_on = std::max(0, notes_on - 1);
      }

      if (notes_on == 0) {
        envelope.note_off();
      }

      const auto env = envelope.tick(info.sample_rate);

      const auto pitch_mod_depth = pitch_modulation_depth.evaluate();
      const auto crm = center_rate_multiplier;
      rate_multiplier = crm + rate_multiplier_lfo.tick() * pitch_mod_depth * crm;

      Sample2 samp{};
      auto interp_info = util::make_linear_interpolation_info(frame_index, num_frames);

      if (chunk.is_in_bounds(interp_info.i0) && chunk.is_in_bounds(interp_info.i1)) {
        samp.samples[0] = util::tick_interpolated_float(chunk, src0, interp_info) * env;
        samp.samples[1] = util::tick_interpolated_float(chunk, src1, interp_info) * env;
      }

      frame_index += frame_index_increment(
        chunk.descriptor.sample_rate, info.sample_rate, rate_multiplier);

      auto delay_mix_val = delay_mix.evaluate();
      auto delayed = rhythmic_delay.tick(samp, delay_time, info.sample_rate, 0.5);
      samp = lerp(delay_mix_val, samp, delayed);

      out0.write(out.buffer.data, i, &samp);
    }
  }

  reverb.process(out, out, events, info);
  last_render_frame = info.render_frame + info.num_frames;
}

OutputAudioPorts ModulatedSampler::outputs() const {
  return output_ports;
}

InputAudioPorts ModulatedSampler::inputs() const {
  return input_ports;
}

void ModulatedSampler::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  (void) node_id;
  reverb.parameter_descriptors(mem);
}

GROVE_NAMESPACE_END
