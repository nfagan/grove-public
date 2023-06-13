#include "ArpeggiatorSystem.hpp"
#include "Transport.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "PitchSamplingSystem.hpp"
#include "arpeggio.hpp"
#include "grove/common/common.hpp"
#include "grove/math/random.hpp"
#include <memory>
#include <vector>

GROVE_NAMESPACE_BEGIN

struct Config {
  static constexpr double tsig_num = double(reference_time_signature().numerator);
  static constexpr int max_num_slots_per_arp = 4;
  static constexpr uint8_t midi_message_source_id = 4;
  static constexpr int max_num_pitch_classes_in_note_params = 24;
  static constexpr int max_num_octaves_in_note_params = 6;
};

struct PitchSamplingSystemNoteParameters {
  bool is_valid() const {
    return group.id > 0;
  }

  PitchSampleSetGroupHandle group;
  uint8_t set;
};

struct NoteSamplingParameters {
  bool empty() const {
    return num_pitch_classes == 0 || num_octaves == 0;
  }

  PitchClass pitch_classes[Config::max_num_pitch_classes_in_note_params];
  uint8_t num_pitch_classes;
  int8_t octaves[Config::max_num_octaves_in_note_params];
  uint8_t num_octaves;
};

struct NoteCyclingParameters {
  bool empty() const {
    return num_base_notes == 0;
  }

  MIDINote base_notes[Config::max_num_slots_per_arp];
  uint8_t num_base_notes;
  uint8_t semitone_step;
  uint8_t num_steps;
};

struct ArpeggiatorParameters {
  bool can_generate_notes(int si) const {
    if (si >= int(num_slots_active)) {
      return false;
    }

    switch (pitch_mode) {
      case ArpeggiatorSystemPitchMode::Random: {
        return !note_sampling_params.empty();
      }
      case ArpeggiatorSystemPitchMode::CycleUp: {
        return !note_cycling_params.empty();
      }
      case ArpeggiatorSystemPitchMode::RandomFromPitchSampleSet:
      case ArpeggiatorSystemPitchMode::CycleUpFromPitchSampleSet: {
        return pitch_sample_params.is_valid();
      }
      default: {
        assert(false);
        return false;
      }
    }
  }

  ArpeggiatorSystemPitchMode pitch_mode;
  ArpeggiatorSystemDurationMode duration_mode;
  NoteSamplingParameters note_sampling_params;
  NoteCyclingParameters note_cycling_params;
  PitchSamplingSystemNoteParameters pitch_sample_params;
  uint8_t num_slots_active;
};

enum class NoteState {
  Inactive,
  PendingActive,
  Active
};

struct NoteSlot {
  bool not_inactive() const {
    return is_active() || is_pending_active();
  }

  bool is_inactive() const {
    return state == NoteState::Inactive;
  }

  bool is_pending_active() const {
    return state == NoteState::PendingActive;
  }

  bool is_active() const {
    return state == NoteState::Active;
  }

  NoteState state;
  MIDINote note;
  ScoreCursor start;
  audio::Quantization quantization;
  double play_for_beats;
  bool is_rest;
};

struct NoteSlotMeta {
  uint8_t cycle_phase;
};

struct ArpeggiatorInstance {
  ArpeggiatorInstanceHandle handle{};
  MIDIMessageStreamHandle midi_message_stream{};
  NoteSlot slots[Config::max_num_slots_per_arp]{};
  NoteSlotMeta slot_meta[Config::max_num_slots_per_arp]{};

  ArpeggiatorParameters render_params{};
  Handshake<ArpeggiatorParameters> handoff_params{};
  ArpeggiatorParameters ui_params{};
  bool ui_params_modified{};
};

using InstanceVec = std::vector<std::shared_ptr<ArpeggiatorInstance>>;

struct Instances {
  ArpeggiatorInstance* find_instance(ArpeggiatorInstanceHandle handle) {
    for (auto& inst : *set0) {
      if (inst->handle == handle) {
        return inst.get();
      }
    }
    return nullptr;
  }

  void destroy(ArpeggiatorInstanceHandle handle) {
    auto it = std::find_if(set0->begin(), set0->end(), [handle](const auto& inst) {
      return inst->handle == handle;
    });
    if (it != set0->end()) {
      set0->erase(it);
      modified = true;
    } else {
      assert(false);
    }
  }

  std::unique_ptr<InstanceVec> set0;
  std::unique_ptr<InstanceVec> set1;
  std::unique_ptr<InstanceVec> set2;
  bool modified{};
};

struct ArpeggiatorSystem {
  std::atomic<bool> initialized{};
  const Transport* transport{};
  MIDIMessageStreamSystem* midi_stream_system{};
  PitchSamplingSystem* pitch_sampling_system{};

  InstanceVec* render_instances{};
  Handshake<InstanceVec*> handoff_instances;

  Instances instances;
  uint32_t next_instance_id{1};
};

namespace {

std::shared_ptr<ArpeggiatorInstance> create_instance(
  ArpeggiatorInstanceHandle handle, MIDIMessageStreamHandle midi_message_stream) {
  //
  auto res = std::make_shared<ArpeggiatorInstance>();
  res->handle = handle;
  res->midi_message_stream = midi_message_stream;
  return res;
}

Instances make_instances() {
  Instances result;
  result.set0 = std::make_unique<InstanceVec>();
  result.set1 = std::make_unique<InstanceVec>();
  result.set2 = std::make_unique<InstanceVec>();
  return result;
}

NoteCyclingParameters make_note_cycling_params(
  const MIDINote* notes, uint8_t num_notes, uint8_t st_step, uint8_t num_steps) {
  //
  assert(num_notes <= Config::max_num_slots_per_arp);
  assert(num_steps > 0);
  NoteCyclingParameters result{};
  for (int i = 0; i < std::min(int(num_notes), Config::max_num_slots_per_arp); i++) {
    result.base_notes[result.num_base_notes++] = notes[i];
  }
  result.semitone_step = st_step;
  result.num_steps = num_steps;
  return result;
};

NoteSamplingParameters make_note_sampling_params(
  const PitchClass* pcs, uint8_t num_pcs, const int8_t* octs, uint8_t num_octs) {
  //
  const int max_pcs = Config::max_num_pitch_classes_in_note_params;
  const int max_octs = Config::max_num_octaves_in_note_params;
  assert(num_pcs <= max_pcs && num_octs <= max_octs);
  NoteSamplingParameters result{};
  for (int i = 0; i < std::min(max_pcs, int(num_pcs)); i++) {
    result.pitch_classes[result.num_pitch_classes++] = pcs[i];
  }
  for (int i = 0; i < std::min(max_octs, int(num_octs)); i++) {
    result.octaves[result.num_octaves++] = octs[i];
  }
  return result;
}

PitchSamplingSystemNoteParameters make_pitch_sampling_params(PitchSampleSetGroupHandle group) {
  PitchSamplingSystemNoteParameters result{};
  result.set = 0; //  @TODO
  result.group = group;
  return result;
}

double block_relative_sample(ScoreCursor loc, const ScoreRegion& block_region, double bps) {
  assert(block_region.contains(loc, Config::tsig_num));
  loc.wrapped_sub_cursor(block_region.begin, Config::tsig_num);
  return loc.to_sample_offset(1.0 / bps, Config::tsig_num);
}

MIDIStreamMessage make_message(int frame, MIDIMessage msg) {
  MIDIStreamMessage res{};
  res.frame = frame;
  res.message = msg;
  res.source_id = Config::midi_message_source_id;
  return res;
}

MIDINote random_note(const ArpeggiatorInstance& inst) {
  const auto& note_p = inst.render_params.note_sampling_params;
  assert(!note_p.empty());
  MIDINote res;
  res.pitch_class = *uniform_array_sample(note_p.pitch_classes, note_p.num_pitch_classes);
  res.octave = *uniform_array_sample(note_p.octaves, note_p.num_octaves);
  res.velocity = 127;
  return res;
}

bool is_existing_note(const ArpeggiatorInstance& inst, int si, MIDINote note) {
  for (int i = 0; i < Config::max_num_slots_per_arp; i++) {
    auto& slot = inst.slots[i];
    if (i != si && slot.not_inactive() && !slot.is_rest && slot.note == note) {
      return true;
    }
  }
  return false;
}

template <typename GenNote>
MIDINote next_note_prefer_new(const ArpeggiatorInstance& inst, int si, const GenNote& gen_note) {
  int attempt{};
  do {
    auto note = gen_note(inst);
    if (!is_existing_note(inst, si, note) || ++attempt == 4) {
      return note;
    }
  } while (true);
}

MIDINote random_next_note(const ArpeggiatorInstance& inst, int si) {
  return next_note_prefer_new(inst, si, [](const ArpeggiatorInstance& inst) {
    return random_note(inst);
  });
}

MIDINote cycle_next_note(ArpeggiatorInstance& inst, int si) {
  auto& cycle_p = inst.render_params.note_cycling_params;
  assert(!cycle_p.empty() && cycle_p.num_steps > 0);
  auto& slot_meta = inst.slot_meta[si];

  const uint32_t ni = uint32_t(si) % uint32_t(cycle_p.num_base_notes);
  const MIDINote base_note = cycle_p.base_notes[ni];
  uint8_t stp = slot_meta.cycle_phase % cycle_p.num_steps;
  slot_meta.cycle_phase = stp + uint8_t(1);

  uint32_t st_off = uint32_t(stp) * uint32_t(cycle_p.semitone_step);
  auto st = uint8_t(base_note.note_number() + st_off);
  return MIDINote::from_note_number(st);
}

MIDINote pitch_sample_next_note(ArpeggiatorSystem* sys, ArpeggiatorInstance& inst, int si) {
  return next_note_prefer_new(inst, si, [sys](const ArpeggiatorInstance& inst) {
    auto& sample_p = inst.render_params.pitch_sample_params;
    assert(sample_p.is_valid());
    return pss::render_uniform_sample_midi_note(
      sys->pitch_sampling_system, sample_p.group, sample_p.set, 3);
  });
}

MIDINote cycle_pitch_sample_next_note(ArpeggiatorSystem* sys, ArpeggiatorInstance& inst, int si) {
  auto& sample_p = inst.render_params.pitch_sample_params;
  auto& slot_meta = inst.slot_meta[si];

  double poss_semitones[64];
  poss_semitones[0] = 0.0;
  int num_sts = std::max(1, pss::render_read_semitones(
    sys->pitch_sampling_system, sample_p.group, sample_p.set, poss_semitones, 64));

  slot_meta.cycle_phase %= uint8_t(num_sts);
  const int cp = slot_meta.cycle_phase;
  slot_meta.cycle_phase += uint8_t(1);

  const auto note_num = uint8_t(int(midi_note_number_c3()) + int(poss_semitones[cp]));
  return MIDINote::from_note_number(note_num);
}

MIDINote next_note(ArpeggiatorSystem* sys, ArpeggiatorInstance& inst, int si) {
  auto& render_p = inst.render_params;
  switch (render_p.pitch_mode) {
    case ArpeggiatorSystemPitchMode::Random: {
      return random_next_note(inst, si);
    }
    case ArpeggiatorSystemPitchMode::CycleUp: {
      return cycle_next_note(inst, si);
    }
    case ArpeggiatorSystemPitchMode::RandomFromPitchSampleSet: {
      return pitch_sample_next_note(sys, inst, si);
    }
    case ArpeggiatorSystemPitchMode::CycleUpFromPitchSampleSet: {
      return cycle_pitch_sample_next_note(sys, inst, si);
    }
    default: {
      assert(false);
      return MIDINote::A4;
    }
  }
}

void set_pending_slot_params(
  ArpeggiatorSystem* sys, ArpeggiatorInstance& inst, NoteSlot& slot, int si) {
  //
  slot.is_rest = false;

  switch (inst.render_params.duration_mode) {
    case ArpeggiatorSystemDurationMode::Random: {
      slot.quantization = audio::Quantization::Eighth;
      slot.play_for_beats = urand() < 0.333 ? 0.5 : 1.0;
      slot.is_rest = urand() < 0.25;
      if (urand() < 0.125) {
        slot.quantization = audio::Quantization::Sixteenth;
        slot.play_for_beats = 0.25;
        slot.is_rest = false;
      }
      break;
    }
    case ArpeggiatorSystemDurationMode::Quarter: {
      slot.play_for_beats = 1.0;
      slot.quantization = audio::Quantization::Quarter;
      break;
    }
    case ArpeggiatorSystemDurationMode::Eighth: {
      slot.play_for_beats = 0.5;
      slot.quantization = audio::Quantization::Eighth;
      break;
    }
    case ArpeggiatorSystemDurationMode::Sixteenth: {
      slot.play_for_beats = 0.25;
      slot.quantization = audio::Quantization::Sixteenth;
      break;
    }
    default: {
      break;
    }
  }

  if (!slot.is_rest) {
    slot.note = next_note(sys, inst, si);
  }
}

struct {
  ArpeggiatorSystem sys;
} globals;

} //  anon

ArpeggiatorSystem* arp::get_global_arpeggiator_system() {
  return &globals.sys;
}

void arp::render_begin_process(ArpeggiatorSystem* sys, const AudioRenderInfo& info) {
  if (!sys->initialized.load()) {
    return;
  }

  if (auto insts = read(&sys->handoff_instances)) {
    sys->render_instances = insts.value();
  }

  for (auto& inst : *sys->render_instances) {
    if (auto params = read(&inst->handoff_params)) {
      inst->render_params = std::move(params.value());
    }
  }

  const Transport* transport = sys->transport;
  const double bps = beats_per_sample_at_bpm(
    transport->get_bpm(), info.sample_rate, reference_time_signature());
  const ScoreRegion block_region{
    transport->render_get_pausing_cursor_location(),
    ScoreCursor::from_beats(bps * info.num_frames, Config::tsig_num)};

  const bool just_stopped = transport->just_stopped();
  const bool playing = transport->render_is_playing();

  for (auto& inst : *sys->render_instances) {
    for (int si = 0; si < Config::max_num_slots_per_arp; si++) {
      auto& slot = inst->slots[si];
      if (just_stopped && slot.is_active()) {
        if (!slot.is_rest) {
          MIDIStreamMessage msg = make_message(0, MIDIMessage::make_note_off(0, slot.note));
          midi::render_push_messages(sys->midi_stream_system, inst->midi_message_stream, &msg, 1);
        }
        slot = {};
      }

      if (!playing) {
        continue;
      }

      ScoreCursor latest_event = block_region.begin;
      while (true) {
        if (slot.is_inactive()) {
          if (!inst->render_params.can_generate_notes(si)) {
            break;
          }
          assert(si < inst->render_params.num_slots_active);
          //  if the slot is not in use, choose parameters for it.
          set_pending_slot_params(sys, *inst, slot, si);
          slot.state = NoteState::PendingActive;
        }

        if (slot.is_pending_active()) {
          assert(slot.start == ScoreCursor{});
          //  if the slot is pending, determine whether to activate it.
          ScoreCursor loc = next_quantum(latest_event, slot.quantization, Config::tsig_num);
          if (block_region.contains(loc, Config::tsig_num)) {
            //  @NOTE: Note on.
            if (!slot.is_rest) {
              const double sample = block_relative_sample(loc, block_region, bps);
              const int frame = std::min(int(sample), info.num_frames - 1);
              MIDIStreamMessage msg = make_message(frame, MIDIMessage::make_note_on(0, slot.note));
              midi::render_push_messages(sys->midi_stream_system, inst->midi_message_stream, &msg, 1);
            }

            slot.start = loc;
            slot.state = NoteState::Active;
          } else {
            break;
          }
        }

        if (slot.is_active()) {
          auto end = slot.start;
          end.wrapped_add_beats(slot.play_for_beats, Config::tsig_num);
          if (block_region.contains(end, Config::tsig_num)) {
            //  @NOTE: Note off.
            if (!slot.is_rest) {
              const double sample = block_relative_sample(end, block_region, bps);
              const int frame = std::min(int(sample), info.num_frames - 1);
              MIDIStreamMessage msg = make_message(frame, MIDIMessage::make_note_off(0, slot.note));
              midi::render_push_messages(sys->midi_stream_system, inst->midi_message_stream, &msg, 1);
            }

            latest_event = end;
            slot = {};
          } else {
            break;
          }
        }
      }
    }
  }
}

uint8_t arp::get_midi_source_id() {
  return Config::midi_message_source_id;
}

void arp::ui_initialize(
  ArpeggiatorSystem* sys, MIDIMessageStreamSystem* midi_stream_sys,
  PitchSamplingSystem* pitch_sampling_sys, const Transport* transport) {
  //
  assert(!sys->initialized.load());
  sys->instances = make_instances();
  sys->render_instances = sys->instances.set2.get();
  sys->transport = transport;
  sys->midi_stream_system = midi_stream_sys;
  sys->pitch_sampling_system = pitch_sampling_sys;
  sys->initialized.store(true);
}

void arp::ui_set_num_active_slots(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, uint8_t num) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  num = uint8_t(std::min(int(num), Config::max_num_slots_per_arp));
  inst->ui_params.num_slots_active = num;
  inst->ui_params_modified = true;
}

void arp::ui_set_pitch_mode(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, ArpeggiatorSystemPitchMode mode) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  inst->ui_params.pitch_mode = mode;
  inst->ui_params_modified = true;
}

void arp::ui_set_duration_mode(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, ArpeggiatorSystemDurationMode mode) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  inst->ui_params.duration_mode = mode;
  inst->ui_params_modified = true;
}

void arp::ui_set_pitch_sample_set_group(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, const PitchSampleSetGroupHandle& pss) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  inst->ui_params.pitch_sample_params = make_pitch_sampling_params(pss);
  inst->ui_params_modified = true;
}

void arp::ui_set_note_sampling_parameters(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp,
  const PitchClass* pcs, uint8_t num_pcs, const int8_t* octs, uint8_t num_octs) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  inst->ui_params.note_sampling_params = make_note_sampling_params(pcs, num_pcs, octs, num_octs);
  inst->ui_params_modified = true;
}

void arp::ui_set_note_cycling_parameters(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp,
  const MIDINote* notes, uint8_t num_notes, uint8_t st_step, uint8_t num_steps) {
  //
  auto* inst = sys->instances.find_instance(arp);
  if (!inst) {
    return;
  }

  num_steps = uint8_t(std::max(1, int(num_steps)));
  inst->ui_params.note_cycling_params = make_note_cycling_params(
    notes, num_notes, st_step, num_steps);
  inst->ui_params_modified = true;
}

void arp::ui_update(ArpeggiatorSystem* sys) {
  for (auto& inst : *sys->instances.set0) {
    if (inst->ui_params_modified && !inst->handoff_params.awaiting_read) {
      ArpeggiatorParameters tmp = inst->ui_params;
      publish(&inst->handoff_params, std::move(tmp));
      inst->ui_params_modified = false;
    }
    if (inst->handoff_params.awaiting_read) {
      (void) acknowledged(&inst->handoff_params);
    }
  }

  if (sys->instances.modified && !sys->handoff_instances.awaiting_read) {
    *sys->instances.set1 = *sys->instances.set0;
    publish(&sys->handoff_instances, sys->instances.set1.get());
    sys->instances.modified = false;
  }

  if (sys->handoff_instances.awaiting_read && acknowledged(&sys->handoff_instances)) {
    std::swap(sys->instances.set1, sys->instances.set2);
  }
}

ArpeggiatorInstanceHandle arp::ui_create_arpeggiator(
  ArpeggiatorSystem* sys, uint32_t midi_message_stream) {
  //
  ArpeggiatorInstanceHandle handle{sys->next_instance_id++};
  auto inst = create_instance(handle, MIDIMessageStreamHandle{midi_message_stream});
  sys->instances.set0->push_back(std::move(inst));
  sys->instances.modified = true;
  return handle;
}

void arp::ui_destroy_arpeggiator(ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle inst) {
  sys->instances.destroy(inst);
}

int arp::ui_get_num_instances(const ArpeggiatorSystem* sys) {
  return int(sys->instances.set0->size());
}

ArpeggiatorInstanceHandle arp::ui_get_ith_instance(const ArpeggiatorSystem* sys, int i) {
  auto& insts = *sys->instances.set0;
  assert(i < int(insts.size()));
  return insts[i]->handle;
}

ReadArpeggiatorState arp::ui_read_state(
  const ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle handle) {
  //
  ReadArpeggiatorState result{};
  for (auto& inst : *sys->instances.set0) {
    if (inst->handle == handle) {
      auto& params = inst->ui_params;
      result.pitch_mode = params.pitch_mode;
      result.duration_mode = params.duration_mode;
      result.num_slots_active = params.num_slots_active;
      break;
    }
  }
  return result;
}

GROVE_NAMESPACE_END
