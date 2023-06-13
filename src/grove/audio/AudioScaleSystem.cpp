#include "AudioScaleSystem.hpp"
#include "scales.hpp"
#include "grove/math/util.hpp"
#include "grove/common/Handshake.hpp"
#include "grove/common/common.hpp"
#include <cstring>

GROVE_NAMESPACE_BEGIN

struct AudioScaleSystem {
  bool render_began_process{};
  int render_scale0_index{};
  int render_scale1_index{};
  float render_frac_scale1{};
  Tuning render_tuning{default_tuning()};
  int render_note_number_offset{};
  double render_rate_multiplier_scale{1.0};
  int num_frames_prepared{};

  std::atomic<float> ui_frac_scale1{};
  std::atomic<int> ui_scale0_index{};
  std::atomic<int> ui_scale1_index{};
  Tuning ui_tuning{default_tuning()};
  Handshake<Tuning> handoff_tuning{};
  Optional<Tuning> ui_pending_send_tuning;
};

namespace {

struct {
  AudioScaleSystem sys;
} globals;

} //  anon

AudioScaleSystem* scale_system::get_global_audio_scale_system() {
  return &globals.sys;
}

void scale_system::render_begin_process(AudioScaleSystem* sys, const AudioRenderInfo& info) {
  sys->render_began_process = false;

  if (auto tuning = read(&sys->handoff_tuning)) {
    sys->render_tuning = tuning.value();

    //  if `ref_st` is higher than the scale reference, then notes should become lower in pitch,
    //  hence the offset is negative.
    const int ref_st = int(sys->render_tuning.reference_semitone);
    sys->render_note_number_offset = int(scales::reference_note_number) - ref_st;
    //  if `reference_frequency` is higher than the scale reference, then notes should become higher
    //  in pitch.
    sys->render_rate_multiplier_scale = sys->render_tuning.reference_frequency / scales::reference_frequency;
  }

  sys->render_frac_scale1 = sys->ui_frac_scale1.load();
  assert(sys->render_frac_scale1 >= 0.0f && sys->render_frac_scale1 <= 1.0f);

  sys->render_scale0_index = sys->ui_scale0_index.load();
  sys->render_scale1_index = sys->ui_scale1_index.load();
  assert(sys->render_scale0_index < scales::num_scales);
  assert(sys->render_scale1_index < scales::num_scales);

  sys->num_frames_prepared = info.num_frames;
  sys->render_began_process = true;
}

double scale_system::render_get_frequency(
  const AudioScaleSystem* sys, uint8_t note_number, int frame) {
  //
  return render_get_rate_multiplier(sys, note_number, frame) * scales::reference_frequency;
}

double scale_system::render_get_rate_multiplier_from_semitone(
  const AudioScaleSystem* sys, double st, int frame) {
  /*
   * st_off = st - floor(st);
   * st = floor(st);
   * rm = 2^(st/12) = render_get_rate_multiplier(...)
   * log2(rm) = st/12
   * st = 12 * log2(rm) + st_off
   */
  assert(std::isfinite(st));
  const double rm = render_get_rate_multiplier(sys, uint8_t(clamp(st, 0.0, 255.0)), frame);
  const double st_off = st - std::floor(st);
  if (st_off == 0.0) {
    return rm;
  }
  st = 12.0 * std::log2(rm) + st_off;
  return std::pow(2.0, st / 12.0);
}

double scale_system::render_get_frequency_from_semitone(
  const AudioScaleSystem* sys, double st, int frame) {
  //
  return render_get_rate_multiplier_from_semitone(sys, st, frame) * scales::reference_frequency;
}

double scale_system::render_get_rate_multiplier(
  const AudioScaleSystem* sys, uint8_t note_number, int frame) {
  //
  if (!sys->render_began_process) {
    return 1.0;
  }

  //  `frame` may be used to smooth changes to `render_frac_scale`.
  assert(frame >= 0 && frame < sys->num_frames_prepared);
  (void) frame;

  const int si0 = sys->render_scale0_index;
  const int si1 = sys->render_scale1_index;
  assert(si0 < scales::num_scales && si1 < scales::num_scales);

  const uint8_t ni_off = uint8_t(clamp(int(note_number) + sys->render_note_number_offset, 0, 255));

  //  @TODO: Linear interpolation between rate multipliers might not be correct - haven't considered
  //  this seriously yet.
  double rm0 = scales::rate_multipliers[si0][ni_off];
  double rm1 = scales::rate_multipliers[si1][ni_off];
  double rm_scale = sys->render_rate_multiplier_scale;
  return rm_scale * lerp(sys->render_frac_scale1, rm0, rm1);
}

const Tuning* scale_system::render_get_tuning(const AudioScaleSystem* sys) {
  return &sys->render_tuning;
}

const Tuning* scale_system::ui_get_tuning(const AudioScaleSystem* sys) {
  return &sys->ui_tuning;
}

void scale_system::ui_set_tuning(AudioScaleSystem* sys, Tuning tuning) {
  sys->ui_tuning = tuning;
  sys->ui_pending_send_tuning = tuning;
}

void scale_system::ui_set_frac_scale1(AudioScaleSystem* sys, float frac) {
  assert(frac >= 0.0f && frac <= 1.0f);
  sys->ui_frac_scale1.store(clamp01(frac));
}

float scale_system::ui_get_frac_scale1(AudioScaleSystem* sys) {
  return sys->ui_frac_scale1.load();
}

Optional<int> scale_system::ui_find_scale_by_name(const AudioScaleSystem*, const char* name) {
  for (int i = 0; i < scales::num_scales; i++) {
    if (std::strcmp(name, scales::names[i]) == 0) {
      return Optional<int>(i);
    }
  }
  return NullOpt{};
}

void scale_system::ui_set_scale_indices(AudioScaleSystem* sys, int scale0, int scale1) {
  assert(scale0 >= 0 && scale0 < scales::num_scales);
  assert(scale1 >= 0 && scale1 < scales::num_scales);
  sys->ui_scale0_index.store(scale0);
  sys->ui_scale1_index.store(scale1);
}

AudioScaleSystemScaleDescriptors
scale_system::ui_get_active_scale_descriptors(const AudioScaleSystem* sys) {
  AudioScaleSystemScaleDescriptors result{};
  result.scales[0] = ui_get_ith_scale_desc(sys, sys->ui_scale0_index.load());
  result.scales[1] = ui_get_ith_scale_desc(sys, sys->ui_scale1_index.load());
  return result;
}

int scale_system::ui_get_num_scales(const AudioScaleSystem*) {
  return scales::num_scales;
}

AudioScaleSystemScaleDescriptor scale_system::ui_get_ith_scale_desc(const AudioScaleSystem*, int index) {
  assert(index >= 0 && index < scales::num_scales);
  AudioScaleSystemScaleDescriptor result{};
  result.index = index;
  result.name = scales::names[index];
  result.num_notes_per_octave = scales::num_notes_per_octave[index];
  return result;
}

AudioScaleSystemScaleDescriptor scale_system::ui_get_ith_active_scale_desc(
  const AudioScaleSystem* sys, int index) {
  //
  assert(index == 0 || index == 1);
  auto scale_descs = ui_get_active_scale_descriptors(sys);
  return scale_descs.scales[index];
}

void scale_system::ui_initialize(AudioScaleSystem* sys) {
  if (auto scale_ind = ui_find_scale_by_name(sys, "12_tet")) {
    ui_set_scale_indices(sys, scale_ind.value(), scale_ind.value());
  }
}

void scale_system::ui_update(AudioScaleSystem* sys) {
  if (sys->handoff_tuning.awaiting_read) {
    (void) acknowledged(&sys->handoff_tuning);
  }

  if (sys->ui_pending_send_tuning && !sys->handoff_tuning.awaiting_read) {
    publish(&sys->handoff_tuning, std::move(sys->ui_pending_send_tuning.value()));
    sys->ui_pending_send_tuning = NullOpt{};
  }
}

GROVE_NAMESPACE_END