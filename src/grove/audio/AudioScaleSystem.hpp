#pragma once

#include "types.hpp"
#include "tuning.hpp"
#include "grove/common/Optional.hpp"

#define GROVE_PREFER_AUDIO_SCALE_SYS (1)

namespace grove {

struct AudioScaleSystem;

struct AudioScaleSystemScaleDescriptor {
  int index;
  const char* name;
  int num_notes_per_octave;
};

struct AudioScaleSystemScaleDescriptors {
  AudioScaleSystemScaleDescriptor scales[2];
};

}

namespace grove::scale_system {

AudioScaleSystem* get_global_audio_scale_system();

void render_begin_process(AudioScaleSystem* sys, const AudioRenderInfo& info);
double render_get_rate_multiplier(const AudioScaleSystem* sys, uint8_t note_number, int frame);
double render_get_frequency(const AudioScaleSystem* sys, uint8_t note_number, int frame);
double render_get_rate_multiplier_from_semitone(const AudioScaleSystem* sys, double st, int frame);
double render_get_frequency_from_semitone(const AudioScaleSystem* sys, double st, int frame);
const Tuning* render_get_tuning(const AudioScaleSystem* sys);

const Tuning* ui_get_tuning(const AudioScaleSystem* sys);
void ui_set_tuning(AudioScaleSystem* sys, Tuning tuning);
Optional<int> ui_find_scale_by_name(const AudioScaleSystem* sys, const char* name);

void ui_set_frac_scale1(AudioScaleSystem* sys, float frac);
float ui_get_frac_scale1(AudioScaleSystem* sys);

AudioScaleSystemScaleDescriptors ui_get_active_scale_descriptors(const AudioScaleSystem* sys);
void ui_set_scale_indices(AudioScaleSystem* sys, int scale0, int scale1);

int ui_get_num_scales(const AudioScaleSystem* sys);
AudioScaleSystemScaleDescriptor ui_get_ith_scale_desc(const AudioScaleSystem* sys, int index);
AudioScaleSystemScaleDescriptor ui_get_ith_active_scale_desc(const AudioScaleSystem* sys, int index);

void ui_initialize(AudioScaleSystem* sys);
void ui_update(AudioScaleSystem* sys);

}