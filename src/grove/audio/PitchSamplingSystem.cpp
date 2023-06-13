#include "PitchSamplingSystem.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

struct Config {
  static constexpr int max_num_groups = 2;
  static constexpr int num_sets_per_group = 4;
  static constexpr int max_num_sets = max_num_groups * num_sets_per_group;
  static constexpr int max_num_semitones_per_set = 32;
  static constexpr int max_num_triggered_semitones = 4;
};

struct SampleSet {
  float semitones[Config::max_num_semitones_per_set];
  int num_semitones;
};

struct UISampleSet {
  SampleSet fixed_set;
  SampleSet triggered_set;
  bool prefer_triggered;
  bool modified;
};

struct SendSampleSet {
  int index;
  SampleSet set;
};

struct PitchSamplingSystem {
  SampleSet render_sets[Config::max_num_sets]{};
  UISampleSet ui_sets[Config::max_num_sets]{};
  RingBuffer<SendSampleSet, 4> send_to_render;

  PitchSampleSetGroupHandle group_handles[Config::max_num_groups]{};
  uint32_t num_groups{};
};

namespace {

int get_sample_set_index(PitchSampleSetGroupHandle group, uint32_t set) {
  assert(group.id > 0);
  const int res = int(group.id - 1) * Config::num_sets_per_group + int(set);
  assert(res >= 0 && res < Config::max_num_sets);
  return res;
}

const SampleSet* render_read_sample_set(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set) {
  //
  return sys->render_sets + get_sample_set_index(group, set);
}

const UISampleSet* ui_read_sample_set(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set) {
  //
  return sys->ui_sets + get_sample_set_index(group, set);
}

PitchSampleSetGroupHandle ui_create_sample_set_group(PitchSamplingSystem* sys) {
  assert(sys->num_groups < Config::max_num_groups);
  PitchSampleSetGroupHandle result{sys->num_groups + 1};  //  @NOTE: 1-based index
  sys->num_groups++;
  return result;
}

MIDINote sample_midi_note_non_empty(const SampleSet& sample_set, int8_t base_octave) {
  assert(sample_set.num_semitones > 0);
  const int st = int(sample_set.semitones[int(urand() * sample_set.num_semitones)]);
  const int8_t oct = int8_t(clamp(st / 12 + int(base_octave), -128, 127));
  auto pc = PitchClass(wrap_within_range(st, 12));

  MIDINote result{};
  result.octave = oct;
  result.pitch_class = pc;
  return result;
}

struct {
  PitchSamplingSystem sys;
} globals;

} //  anon

PitchSamplingSystem* pss::get_global_pitch_sampling_system() {
  return &globals.sys;
}

void pss::render_begin_process(PitchSamplingSystem* sys, const AudioRenderInfo&) {
  const int num_read = sys->send_to_render.size();
  for (int i = 0; i < num_read; i++) {
    SendSampleSet sent = sys->send_to_render.read();
    assert(sent.index >= 0 && sent.index < Config::max_num_sets);
    sys->render_sets[sent.index] = sent.set;
  }
}

double pss::render_uniform_sample_semitone(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, double dflt) {
  //
  if (!group.id) {
    return dflt;
  }

  auto& slot = *render_read_sample_set(sys, group, set);
  if (slot.num_semitones == 0) {
    return dflt;
  } else {
    int ind = int(urand() * slot.num_semitones);
    return slot.semitones[ind];
  }
}

MIDINote pss::render_uniform_sample_midi_note(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, int8_t base_oct) {
  //
  if (!group.id) {
    return MIDINote{PitchClass::C, base_oct, 0};
  }

  auto& slot = *render_read_sample_set(sys, group, set);
  if (slot.num_semitones == 0) {
    return MIDINote{PitchClass::C, base_oct, 0};
  } else {
    return sample_midi_note_non_empty(slot, base_oct);
  }
}

int pss::render_read_semitones(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  double* dst, int max_num_dst) {
  //
  if (!group.id) {
    return 0;
  }

  auto& slot = *render_read_sample_set(sys, group, set);
  const int n = std::min(max_num_dst, slot.num_semitones);
  for (int i = 0; i < n; i++) {
    dst[i] = slot.semitones[i];
  }

  return n;
}

MIDINote pss::ui_uniform_sample_midi_note(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  int8_t base_octave, MIDINote dflt) {
  //
  if (!group.id) {
    return dflt;
  }

  auto& slot = *ui_read_sample_set(sys, group, set);
  const auto& sample_set = slot.prefer_triggered ? slot.triggered_set : slot.fixed_set;

  if (sample_set.num_semitones == 0) {
    return dflt;
  }

  return sample_midi_note_non_empty(sample_set, base_octave);
}

int pss::ui_read_unique_pitch_classes_in_sample_set(
  const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, PitchClass* pcs) {
  //
  auto& ui = sys->ui_sets[get_sample_set_index(group, set)];

  int ct{};
  for (int i = 0; i < 12; i++) {
    for (int s = 0; s < ui.fixed_set.num_semitones; s++) {
      int st = wrap_within_range(int(ui.fixed_set.semitones[s]), 12);
      if (st == i) {
        pcs[ct++] = PitchClass(i);
        break;
      }
    }
  }

  assert(ct <= 12);
  return ct;
}

void pss::ui_push_triggered_semitones(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group,
  uint32_t set, const float* sts, int num_notes) {
  //
  auto& ui = sys->ui_sets[get_sample_set_index(group, set)];
  auto& sample_set = ui.triggered_set;

  for (int i = 0; i < num_notes; i++) {
    float* beg = sample_set.semitones;
    float* end = beg + sample_set.num_semitones;
    float* it = std::find(beg, end, sts[i]);
    if (it == end) {
      if (sample_set.num_semitones < Config::max_num_triggered_semitones) {
        beg[sample_set.num_semitones++] = sts[i];
      } else {
        std::rotate(beg, beg + 1, end);
        beg[Config::max_num_triggered_semitones - 1] = sts[i];
      }
    }
  }

  //  @NOTE: Modify only if we prefer to use triggered notes.
  ui.modified = ui.prefer_triggered;
}

void pss::ui_push_triggered_notes(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const MIDINote* notes, int num_notes, MIDINote ref_note) {
  //
  auto ref_st = int(ref_note.pitch_class) + 12 * int(ref_note.octave);
  for (int i = 0; i < num_notes; i++) {
    auto st = int(notes[i].pitch_class) + 12 * int(notes[i].octave);
    auto push_st = float(st - ref_st);
    ui_push_triggered_semitones(sys, group, set, &push_st, 1);
  }
}

void pss::ui_push_triggered_note_numbers(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const uint8_t* note_nums, int num_notes, uint8_t ref_note_number) {
  //
  for (int i = 0; i < num_notes; i++) {
    float st = float(note_nums[i]) - float(ref_note_number);
    ui_push_triggered_semitones(sys, group, set, &st, 1);
  }
}

void pss::ui_set_prefer_triggered_sample_set(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set, bool prefer_triggered) {
  //
  auto& ui = sys->ui_sets[get_sample_set_index(group, set)];
  if (ui.prefer_triggered != prefer_triggered) {
    ui.prefer_triggered = prefer_triggered;
    ui.modified = true;
  }
}

bool pss::ui_prefers_triggered_sample_set(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set) {
  //
  return sys->ui_sets[get_sample_set_index(group, set)].prefer_triggered;
}

void pss::ui_set_sample_set_from_semitones(
  PitchSamplingSystem* sys, PitchSampleSetGroupHandle group, uint32_t set,
  const float* sts, int num_notes) {
  //
  assert(num_notes < Config::max_num_semitones_per_set);
  num_notes = std::min(num_notes, Config::max_num_semitones_per_set);

  auto& ui = sys->ui_sets[get_sample_set_index(group, set)];
  for (int i = 0; i < num_notes; i++) {
    ui.fixed_set.semitones[i] = sts[i];
  }
  ui.fixed_set.num_semitones = num_notes;
  //  @NOTE: Modify only if we prefer to use triggered fixed set of notes.
  ui.modified = !ui.prefer_triggered;
}

void pss::ui_initialize(PitchSamplingSystem* sys) {
  for (auto& group_handle : sys->group_handles) {
    group_handle = ui_create_sample_set_group(sys);

    float init_offsets[3]{0.0f, -12.0f, 12.0f};
    for (int i = 0; i < Config::num_sets_per_group; i++) {
      ui_set_sample_set_from_semitones(sys, group_handle, i, init_offsets, 3);
    }
  }
}

void pss::ui_update(PitchSamplingSystem* sys) {
  const int num_sets = int(sys->num_groups) * Config::num_sets_per_group;
  for (int i = 0; i < num_sets; i++) {
    auto& ui = sys->ui_sets[i];
    if (!ui.modified) {
      continue;
    }

    SendSampleSet send{};
    send.index = i;
    if (ui.prefer_triggered) {
      send.set = ui.triggered_set;
    } else {
      send.set = ui.fixed_set;
    }

    if (sys->send_to_render.maybe_write(std::move(send))) {
      ui.modified = false;
    } else {
      break;
    }
  }
}

PitchSampleSetGroupHandle pss::ui_get_ith_group(const PitchSamplingSystem* sys, uint32_t i) {
  assert(i < sys->num_groups);
  return sys->group_handles[i];
}

int pss::ui_get_num_groups(const PitchSamplingSystem* sys) {
  return int(sys->num_groups);
}

int pss::ui_get_num_sets_in_group(const PitchSamplingSystem* sys, PitchSampleSetGroupHandle group) {
  (void) sys;
  (void) group;
  return Config::num_sets_per_group;
}

GROVE_NAMESPACE_END
