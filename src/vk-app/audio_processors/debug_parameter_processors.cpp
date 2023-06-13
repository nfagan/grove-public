#include "debug_parameter_processors.hpp"
#include "grove/common/common.hpp"
#include <iostream>

#define COLLAPSE_TO_SINGLE_CHANGE (0)
#define ALTERNATE_PER_FRAME_METHOD (0)
#define INCLUDE_LFO (1)
#define USE_TEST_COMPLETE_FAST_PATH (0)

GROVE_NAMESPACE_BEGIN

/*
 * ExampleParameterizedEffect
 */

ExampleParameterizedEffect::ExampleParameterizedEffect(AudioParameterID node_id) :
  node_id{node_id},
  gain(float(amplitude_to_db(1.0))),
  lfo_depth(1.0f),
  lfo_freq(LfoFreqLimits::min),
  lfo(default_sample_rate(), LfoFreqLimits::min) {
  //

  lfo.fill_sin();
  lfo.normalize();
}

bool ExampleParameterizedEffect::parameter_changes_complete() const {
  return gain.change_complete() && lfo_depth.change_complete() &&
         lfo_freq.change_complete() && waveform_type.change_complete();
}

AudioParameterID ExampleParameterizedEffect::parameter_parent_id() const {
  return node_id;
}

AudioParameterDescriptors ExampleParameterizedEffect::parameter_descriptors() const {
  AudioParameterDescriptors descriptors;
  auto parent_id = node_id;

  AudioParameterIDs gain_ids{parent_id, 0};
  const float gain_min = GainLimits::min;
  const float gain_max = GainLimits::max;
  const float gain_dflt = gain_max;

  auto gain_descriptor =
    make_audio_parameter_descriptor(gain_ids, gain_dflt, gain_min, gain_max, "gain");
  descriptors.push_back(gain_descriptor);

  AudioParameterIDs lfo_depth_ids{parent_id, 1};
  const auto lfo_depth_min = lfo_depth.limits.minimum();
  const auto lfo_depth_max = lfo_depth.limits.maximum();
  const auto lfo_depth_dflt = lfo_depth_max;

  auto lfo_depth_descriptor =
    make_audio_parameter_descriptor(
      lfo_depth_ids, lfo_depth_dflt, lfo_depth_min, lfo_depth_max, "lfo_depth");
  descriptors.push_back(lfo_depth_descriptor);

  AudioParameterIDs lfo_freq_ids{parent_id, 2};
  const auto lfo_freq_min = lfo_freq.limits.minimum();
  const auto lfo_freq_max = lfo_freq.limits.maximum();
  const auto lfo_freq_dflt = lfo_freq_min;

  auto lfo_freq_descriptor =
    make_audio_parameter_descriptor(
      lfo_freq_ids, lfo_freq_dflt, lfo_freq_min, lfo_freq_max, "lfo_frequency");
  descriptors.push_back(lfo_freq_descriptor);

  AudioParameterIDs waveform_type_ids{parent_id, 3};
  const auto waveform_min = waveform_type.limits.minimum();
  const auto waveform_max = waveform_type.limits.maximum();
  const auto waveform_dflt = waveform_min;

  auto waveform_descriptor =
    make_audio_parameter_descriptor(
      waveform_type_ids, waveform_dflt, waveform_min, waveform_max, "waveform_type");
  descriptors.push_back(waveform_descriptor);

  assert(waveform_descriptor.type == AudioParameterType::Int);

  return descriptors;
}

bool ExampleParameterizedEffect::is_enabled() const {
  return true;
}

void ExampleParameterizedEffect::disable() {
  //
}

void ExampleParameterizedEffect::enable() {
  //
}

void ExampleParameterizedEffect::process_without_parameters(Sample* samples,
                                                            AudioEvents*,
                                                            const AudioRenderInfo& info) {
  auto lfo_freq_val = lfo_freq.value;
  auto lfo_depth_val = lfo_depth.value;
  auto gain_val = gain.value;

  lfo.set_sample_rate(info.sample_rate);

  for (int i = 0; i < info.num_frames; i++) {
    lfo.set_frequency(lfo_freq_val);

    const auto lfo_amp = lfo.tick() * lfo_depth_val;
    const auto amp = Sample(db_to_amplitude(gain_val) * lfo_amp);

    for (int j = 0; j < info.num_channels; j++) {
      samples[i * info.num_channels + j] *= amp;
    }
  }
}

void ExampleParameterizedEffect::process(Sample* samples,
                                         AudioEvents*,
                                         const AudioParameterChangeView& parameter_changes,
                                         const AudioRenderInfo& info) {
#if USE_TEST_COMPLETE_FAST_PATH
  if (parameter_changes.empty() && parameter_changes_complete()) {
    std::cout << "Using fast pasth" << std::endl;
    process(samples, events, info);
    return;
  }
#endif

  lfo.set_sample_rate(info.sample_rate);

  auto gain_view = parameter_changes.view_by_parameter(0);
  auto lfo_depth_view = parameter_changes.view_by_parameter(1);
  auto lfo_freq_view = parameter_changes.view_by_parameter(2);
  auto waveform_type_view = parameter_changes.view_by_parameter(3);

#if COLLAPSE_TO_SINGLE_CHANGE
  AudioParameterChange single_gain_change;
  AudioParameterChange single_lfo_depth_change;
  AudioParameterChange single_lfo_freq_change;
  AudioParameterChange single_waveform_change;

  if (gain_view.collapse_to_single_change(&single_gain_change)) {
    gain.apply(single_gain_change);
  }
  if (lfo_depth_view.collapse_to_single_change(&single_lfo_depth_change)) {
    lfo_depth.apply(single_lfo_depth_change);
  }
  if (lfo_freq_view.collapse_to_single_change(&single_lfo_freq_change)) {
    lfo_freq.apply(single_lfo_freq_change);
  }
  if (waveform_type_view.collapse_to_single_change(&single_waveform_change)) {
    waveform_type.apply(single_waveform_change);
  }

  for (int i = 0; i < info.num_frames; i++) {
    const auto curr_type = waveform_type.value;
    const auto new_type = waveform_type.evaluate();

    if (new_type != curr_type) {
      switch (new_type) {
        case 0:
          lfo.fill_sin();
          break;
        case 1:
          lfo.fill_tri(4);
          break;
        case 2:
          lfo.fill_square(4);
          break;
        default:
          assert(false);
      }
      lfo.normalize();
    }

#if INCLUDE_LFO
    lfo.set_frequency(lfo_freq.evaluate());
    const auto lfo_amp = lfo.tick() * lfo_depth.evaluate();
    const auto amp = db_to_amplitude(gain.evaluate()) * lfo_amp;
#else
    const auto amp = db_to_amplitude(gain.evaluate());
#endif

    for (int j = 0; j < info.num_channels; j++) {
      const int ind = i * info.num_channels + j;
      samples[ind] *= amp;
    }
  }
#else

#if ALTERNATE_PER_FRAME_METHOD == 1
  //  0.63, 0.66, 0.68, 0.7
  int gain_view_ind = 0;
  int lfo_depth_ind = 0;
  int lfo_freq_ind = 0;
  int waveform_type_ind = 0;

  int gain_change_frame = -1;
  const AudioParameterChange* gain_change;
  const auto num_gain = gain_view.size();

  int lfo_depth_change_frame = -1;
  const AudioParameterChange* lfo_depth_change;
  const auto num_lfo_depth = lfo_depth_view.size();

  int lfo_freq_change_frame = -1;
  const AudioParameterChange* lfo_freq_change;
  const auto num_lfo_freq = lfo_freq_view.size();

  int waveform_change_frame = -1;
  const AudioParameterChange* waveform_change;
  const auto num_waveform = waveform_type_view.size();

#elif ALTERNATE_PER_FRAME_METHOD == 2
  using ChangeFunc = std::function<void(const AudioParameterChange&)>;
  //  0.7, 0.71, 0.68, 0.66, 0.7, 0.66 -- == 1
  constexpr int num_params = 4;

  int view_indices[num_params] = {0, 0, 0, 0};
  int change_frames[num_params] = {-1, -1, -1, -1};
  const AudioParameterChange* changes[num_params];
  const AudioParameterChange* views[num_params] = {
    gain_view.begin, lfo_depth_view.begin,
    lfo_freq_view.begin, waveform_type_view.begin
  };
  const int view_sizes[num_params] = {
    int(gain_view.size()), int(lfo_depth_view.size()),
    int(lfo_freq_view.size()), int(waveform_type_view.size())
  };
  const ChangeFunc change_funcs[num_params] = {
    [this](const AudioParameterChange& change) { gain.apply(change); },
    [this](const AudioParameterChange& change) { lfo_depth.apply(change); },
    [this](const AudioParameterChange& change) { lfo_freq.apply(change); },
    [this](const AudioParameterChange& change) { waveform_type.apply(change); }
  };

#else
  int gain_view_ind = 0;
  int lfo_depth_ind = 0;
  int lfo_freq_ind = 0;
  int waveform_type_ind = 0;
#endif

  for (int i = 0; i < info.num_frames; i++) {
#if ALTERNATE_PER_FRAME_METHOD == 1
    if (gain_view_ind < num_gain) {
      gain_change = gain_view.begin + gain_view_ind;
      gain_change_frame = gain_change->at_frame;
      assert(gain_change->value.descriptor.is_float());
    }
    if (lfo_depth_ind < num_lfo_depth) {
      lfo_depth_change = lfo_depth_view.begin + lfo_depth_ind;
      lfo_depth_change_frame = lfo_depth_change->at_frame;
      assert(lfo_depth_change->value.descriptor.is_float());
    }
    if (lfo_freq_ind < num_lfo_freq) {
      lfo_freq_change = lfo_freq_view.begin + lfo_freq_ind;
      lfo_freq_change_frame = lfo_freq_change->at_frame;
      assert(lfo_freq_change->value.descriptor.is_float());
    }
    if (waveform_type_ind < num_waveform) {
      waveform_change = waveform_type_view.begin + waveform_type_ind;
      waveform_change_frame = waveform_change->at_frame;
      assert(waveform_change->value.descriptor.is_int());
    }

    if (i == gain_change_frame) {
      gain.change(*gain_change);
      gain_view_ind++;
    }
    if (i == lfo_depth_change_frame) {
      lfo_depth.change(*lfo_depth_change);
      lfo_depth_ind++;
    }
    if (i == lfo_freq_change_frame) {
      lfo_freq.change(*lfo_freq_change);
      lfo_freq_ind++;
    }
    if (i == waveform_change_frame) {
      waveform_type.change(*waveform_change);
      waveform_type_ind++;
    }
#elif ALTERNATE_PER_FRAME_METHOD == 2
    for (int j = 0; j < num_params; j++) {
      if (view_indices[j] < view_sizes[j]) {
        changes[j] = views[j] + view_indices[j];
        change_frames[j] = changes[j]->at_frame;
      }
      if (i == change_frames[j]) {
        change_funcs[j](*changes[j]);
        view_indices[j]++;
      }
    }
#else
    if (gain_view.should_change_now(gain_view_ind, i)) {
      const auto& gain_change = gain_view[gain_view_ind++];
      gain.apply(gain_change);
    }
    if (lfo_depth_view.should_change_now(lfo_depth_ind, i)) {
      const auto& depth_change = lfo_depth_view[lfo_depth_ind++];
      lfo_depth.apply(depth_change);
    }
    if (lfo_freq_view.should_change_now(lfo_freq_ind, i)) {
      const auto& freq_change = lfo_freq_view[lfo_freq_ind++];
      lfo_freq.apply(freq_change);
    }
    if (waveform_type_view.should_change_now(waveform_type_ind, i)) {
      const auto& wave_change = waveform_type_view[waveform_type_ind++];
      waveform_type.apply(wave_change);
    }
#endif

    const auto curr_type = waveform_type.value;
    const auto new_type = waveform_type.evaluate();

    if (new_type != curr_type) {
      switch (new_type) {
        case 0:
          lfo.fill_sin();
          break;
        case 1:
          lfo.fill_tri(4);
          break;
        case 2:
          lfo.fill_square(4);
          break;
        default:
          assert(false);
      }
      lfo.normalize();
    }

#if INCLUDE_LFO
    lfo.set_frequency(lfo_freq.evaluate());
    const auto lfo_amp = lfo.tick() * lfo_depth.evaluate();
    const auto amp = db_to_amplitude(gain.evaluate()) * lfo_amp;
#else
    const auto amp = db_to_amplitude(gain.evaluate());
#endif

    for (int j = 0; j < info.num_channels; j++) {
      samples[i * info.num_channels + j] *= Sample(amp);
    }
  }
#endif
}

GROVE_NAMESPACE_END