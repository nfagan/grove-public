#include "RandomizedInstrument.hpp"
#include "grove/common/common.hpp"
#include "grove/common/util.hpp"
#include "grove/math/random.hpp"
#include <iostream>

#define USE_ENV_REP_PERIOD (0)
#define EMIT_EVENTS (1)

GROVE_NAMESPACE_BEGIN

namespace {

using PitchClasses = DynamicArray<PitchClass, 8>;
using Octaves = DynamicArray<int8_t, 8>;

PitchClass int_to_pitch_class(int v, int off = 0) {
  return PitchClass(wrap_within_range(v + off, 12));
}

void default_octaves(Octaves& octaves) {
  octaves.push_back(int8_t(3));
  octaves.push_back(int8_t(3));
  octaves.push_back(int8_t(4));
  octaves.push_back(int8_t(5));
}

void minor_key1(PitchClasses& pitch_classes, Octaves& octaves, int off) {
  pitch_classes.push_back(int_to_pitch_class(2, off));
  pitch_classes.push_back(int_to_pitch_class(5, off));
  pitch_classes.push_back(int_to_pitch_class(10, off));

  default_octaves(octaves);
}

void minor_key2(PitchClasses& pitch_classes, Octaves& octaves, int off) {
  pitch_classes.push_back(int_to_pitch_class(3, off));
  pitch_classes.push_back(int_to_pitch_class(7, off));

  default_octaves(octaves);
}

void minor_key3(PitchClasses& pitch_classes, Octaves& octaves, int off) {
  pitch_classes.push_back(int_to_pitch_class(3, off));
  pitch_classes.push_back(int_to_pitch_class(5, off));
  pitch_classes.push_back(int_to_pitch_class(7, off));
  pitch_classes.push_back(int_to_pitch_class(9, off));
  pitch_classes.push_back(int_to_pitch_class(10, off));

  default_octaves(octaves);
}

} //  anon

RandomizedInstrument::RandomizedInstrument(AudioParameterID node_id) : node_id{node_id} {
  for (int i = 0; i < num_voices; i++) {
    oscillators.push_back(osc::WaveTable{44.1e3, 1.0});
    oscillators.back().fill_sin();
    oscillators.back().normalize();
    envelopes.emplace_back();
    envelope_representations.emplace_back(default_gain);

    Envelope::Params env_params{};
    env_params.attack_time = 2.0;
    env_params.decay_time = 2.0;
    env_params.sustain_time = 1.0;
    env_params.release_time = 1.0;
    env_params.infinite_sustain = false;
    envelopes.back().configure(env_params);
  }

  randomize_frequencies(0, 0);

  amp_mod_lfo.fill_sin();
  amp_mod_lfo.normalize();
}

void RandomizedInstrument::randomize_frequencies(int key, int off) {
  PitchClasses pitch_classes;
  Octaves octaves;

  switch (key) {
    case 0: {
      minor_key1(pitch_classes, octaves, off);
      break;
    }
    case 1: {
      minor_key2(pitch_classes, octaves, off);
      break;
    }
    case 2: {
      minor_key3(pitch_classes, octaves, off);
      break;
    }
  }

  if (!pitch_classes.empty() && !octaves.empty()) {
    for (auto& osc : oscillators) {
      auto pitch_index = int(float(pitch_classes.size()) * urand());
      auto oct_index = int(float(octaves.size()) * urand());

      MIDINote note{pitch_classes[pitch_index], octaves[oct_index], 127};
      osc.set_frequency(note.frequency());
    }
  }
}

void RandomizedInstrument::apply_new_waveform_type(int new_waveform_type) {
  for (auto& osc : oscillators) {
    switch (new_waveform_type) {
      case 0: {
        osc.fill_sin();
        break;
      }
      case 1: {
        osc.fill_square(4);
        break;
      }
      case 2: {
        osc.fill_square(8);
        break;
      }
    }

    osc.normalize();
  }
}

void RandomizedInstrument::process(const InputData& in,
                                   const OutputData& out,
                                   const AudioRenderInfo& info) {
  for (auto& osc : oscillators) {
    osc.set_sample_rate(info.sample_rate);
  }

  for (auto& env : envelopes) {
    env.set_sample_rate(info.sample_rate);
    if (env.elapsed() && grove::urand() > 0.95) {
      env.note_on();
    }
  }

  amp_mod_lfo.set_sample_rate(info.sample_rate);

  auto waveform_type_view = in.parameter_changes.view_by_parameter(0);
  auto note_key_view = in.parameter_changes.view_by_parameter(1);
  auto note_off_view = in.parameter_changes.view_by_parameter(2);
  auto mod_depth_view = in.parameter_changes.view_by_parameter(3);
  auto gain_view = in.parameter_changes.view_by_parameter(4);

  int waveform_type_ind = 0;
  int note_key_ind = 0;
  int note_off_ind = 0;
  int mod_depth_ind = 0;
  int gain_ind = 0;

  const double amp_factor = 1.0 / double(info.num_channels);

  for (int i = 0; i < info.num_frames; i++) {
    Sample sample{0};

    auto last_waveform_type = waveform_type.value;
    maybe_apply_change(waveform_type_view, waveform_type_ind, waveform_type, i);
    auto new_waveform_type = waveform_type.evaluate();

    auto last_key = note_key.value;
    maybe_apply_change(note_key_view, note_key_ind, note_key, i);
    auto new_key = note_key.evaluate();

    auto last_off = note_offset.value;
    maybe_apply_change(note_off_view, note_off_ind, note_offset, i);
    auto note_off = note_offset.evaluate();

    maybe_apply_change(mod_depth_view, mod_depth_ind, amp_mod_lfo_depth, i);
    auto amp_mod_depth = amp_mod_lfo_depth.evaluate();
    auto amp_mod = amp_mod_lfo.tick();

    maybe_apply_change(gain_view, gain_ind, gain, i);
    auto gain_val = db_to_amplitude(gain.evaluate());

    if (new_key != last_key || last_off != note_off) {
      randomize_frequencies(new_key, note_off);
    }

    if (new_waveform_type != last_waveform_type) {
      apply_new_waveform_type(new_waveform_type);
    }

    for (int j = 0; j < int(oscillators.size()); j++) {
      auto& osc = oscillators[j];

      auto env_amp = envelopes[j].tick();
      auto osc_val = osc.tick();

      auto unmodulated_val = osc_val * env_amp * gain_val * amp_factor;
      auto modulated_val = unmodulated_val * amp_mod;
      auto val = lerp(amp_mod_depth, unmodulated_val, modulated_val);

      sample += Sample(val);
    }

    for (int j = 0; j < info.num_channels; j++) {
      out.samples[i * info.num_channels + j] += sample;
    }
  }


#if EMIT_EVENTS
  if (info.num_frames > 0) {
    int write_frame = 0;
    const int frame_dist = info.num_frames;

    for (int i = 0; i < num_voices; i++) {
      const auto& env = envelopes[i];

      AudioParameterIDs ids{node_id, AudioParameterID(5 + i)};
      auto param_val = make_float_parameter_value(float(env.get_current_gain()));
      auto change = make_audio_parameter_change(ids, param_val, write_frame, frame_dist);
      auto event_data = make_audio_event_data(change);
      auto event = make_audio_event(AudioEvent::Type::NewAudioParameterValue, event_data);

      out.events[write_frame].push_back(event);
    }
  }
#endif
}

AudioParameterDescriptors RandomizedInstrument::parameter_descriptors() const {
  AudioParameterDescriptors descriptors;

  descriptors.push_back(waveform_type.make_descriptor(node_id, 0, 0, "waveform_type"));
  descriptors.push_back(note_key.make_descriptor(node_id, 1, 0, "note_key"));
  descriptors.push_back(note_offset.make_descriptor(node_id, 2, 0, "note_offset"));
  descriptors.push_back(amp_mod_lfo_depth.make_descriptor(node_id, 3, 0.0f, "lfo_depth"));
  descriptors.push_back(gain.make_descriptor(node_id, 4, default_gain, "gain"));

  AudioParameterID env_id{5};
  for (const auto& env : envelope_representations) {
    AudioParameterDescriptor::Flags flags{};
    flags.mark_non_editable();
    flags.mark_monitorable();
    descriptors.push_back(env.make_descriptor(
      node_id, env_id++, default_gain, "envelope_representation", flags));
  }

  return descriptors;
}

AudioParameterID RandomizedInstrument::parameter_parent_id() const {
  return node_id;
}

GROVE_NAMESPACE_END
