#include "NoteClipSystem.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using Instance = NoteClipSystem::Instance;

[[maybe_unused]] bool is_valid_note(const ClipNote& note) {
  return note.span.size > ScoreCursor{} &&
         note.span.begin >= ScoreCursor{} &&
         note.span.begin.beat < reference_time_signature().numerator;
}

NoteClip make_note_clip(NoteClipHandle handle,
                        NoteQueryAcceleratorInstanceHandle note_accel_instance, ScoreRegion span) {
  NoteClip result{};
  result.handle = handle;
  result.note_accel_instance = note_accel_instance;
  result.span = span;
  return result;
}

NoteClipHandle next_clip_handle(NoteClipSystem* sys) {
  NoteClipHandle res{sys->next_clip_id++};
  return res;
}

void push_modification(NoteClipSystem* sys, const NoteClipModification& mod) {
  sys->mods1.push_back(mod);
}

NoteClip* find_clip(std::vector<NoteClip>& clips, NoteClipHandle handle, int* index) {
  for (auto& clip : clips) {
    if (clip.handle == handle) {
      *index = int(&clip - clips.data());
      return &clip;
    }
  }
  return nullptr;
}

NoteClip* find_clip(std::vector<NoteClip>& clips, NoteClipHandle handle) {
  int ignore{};
  return find_clip(clips, handle, &ignore);
}

NoteClipModification make_create_clip_modification(const ScoreRegion& region,
                                                   NoteClipHandle handle) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::CreateClip;
  result.create_clip.region = region;
  result.create_clip.handle = handle;
  return result;
}

NoteClipModification make_clone_clip_modification(NoteClipHandle src, NoteClipHandle dst) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::CloneClip;
  result.clone_clip.src = src;
  result.clone_clip.dst = dst;
  return result;
}

NoteClipModification make_destroy_clip_modification(NoteClipHandle target) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::DestroyClip;
  result.destroy_clip.handle = target;
  return result;
}

NoteClipModification make_add_note_modification(NoteClipHandle target, const ClipNote& note) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::AddNote;
  result.add_note.note = note;
  result.add_note.target = target;
  return result;
}

NoteClipModification make_remove_note_modification(NoteClipHandle target, const ClipNote& note) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::RemoveNote;
  result.remove_note.note = note;
  result.remove_note.target = target;
  return result;
}

NoteClipModification make_modify_note_modification(NoteClipHandle target, const ClipNote& src,
                                                   const ClipNote& dst) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::ModifyNote;
  result.modify_note.src = src;
  result.modify_note.dst = dst;
  result.modify_note.target = target;
  return result;
}

NoteClipModification make_modify_clip_modification(NoteClipHandle target, const ScoreRegion& span) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::ModifyClip;
  result.modify_clip.target = target;
  result.modify_clip.span = span;
  return result;
}

NoteClipModification make_remove_all_notes_modification(NoteClipHandle target) {
  NoteClipModification result{};
  result.type = NoteClipModification::Type::RemoveAllNotes;
  result.remove_all_notes.target = target;
  return result;
}

void create_clip(Instance& inst, const NoteClipModification& mod) {
  auto note_accel_inst = create_note_query_accelerator_instance(&inst.note_accel);
  auto clip_handle = mod.create_clip.handle;
  auto clip = make_note_clip(clip_handle, note_accel_inst, mod.create_clip.region);
  inst.clips.push_back(clip);
}

void clone_clip(Instance& inst, const NoteClipModification& mod) {
  auto* src = find_clip(inst.clips, mod.clone_clip.src);
  auto note_accel_inst = clone_note_query_accelerator_instance(
    &inst.note_accel, src->note_accel_instance);
  auto clip = make_note_clip(mod.clone_clip.dst, note_accel_inst, src->span);
  inst.clips.push_back(clip);
}

void destroy_clip(Instance& inst, const NoteClipModification& mod) {
  int clip_ind{};
  auto* src = find_clip(inst.clips, mod.destroy_clip.handle, &clip_ind);
  destroy_note_query_accelerator_instance(&inst.note_accel, src->note_accel_instance);
  inst.clips.erase(inst.clips.begin() + clip_ind);
}

void remove_note(Instance& inst, NoteClip* clip, const ClipNote& note) {
  grove::remove_note(&inst.note_accel, clip->note_accel_instance, note);
}

void remove_all_notes(Instance& inst, NoteClip* clip) {
  grove::remove_all_notes(&inst.note_accel, clip->note_accel_instance);
}

void insert_note(Instance& inst, NoteClip* clip, const ClipNote& note) {
  grove::insert_note(&inst.note_accel, clip->note_accel_instance, note);
}

using ClipNoteView = ArrayView<const ClipNote>;

ClipNoteView find_intersecting(Instance& inst, NoteClip* clip,
                               const ClipNote& note,
                               TemporaryView<ClipNote>& tmp_notes,
                               TemporaryView<uint32_t>& tmp_inds) {
  return collect_notes_intersecting_note(
    &inst.note_accel,
    read_note_query_tree(&inst.note_accel, clip->note_accel_instance),
    note.span, note.note, tmp_inds, tmp_notes);
}

void remove_intersecting(Instance& inst, NoteClip* clip, const ClipNoteView& notes) {
  for (auto& note : notes) {
    remove_note(inst, clip, note);
  }
}

void add_note(Instance& inst, const NoteClipModification& mod) {
  auto& add_note = mod.add_note;
  auto note = add_note.note;
  auto* clip = find_clip(inst.clips, add_note.target);

  Temporary<ClipNote, 1024> tmp_notes;
  Temporary<uint32_t, 1024> tmp_inds;
  auto tmp_note_view = tmp_notes.view();
  auto tmp_ind_view = tmp_inds.view();
  auto isecting = find_intersecting(inst, clip, note, tmp_note_view, tmp_ind_view);

  remove_intersecting(inst, clip, isecting);
  insert_note(inst, clip, note);
}

void remove_note(Instance& inst, const NoteClipModification& mod) {
  auto& rem_note = mod.remove_note;
  auto note = rem_note.note;
  auto* clip = find_clip(inst.clips, rem_note.target);
  remove_note(inst, clip, note);
}

void modify_note(Instance& inst, const NoteClipModification& mod) {
  auto& modify_note = mod.modify_note;
  auto src = modify_note.src;
  auto dst = modify_note.dst;
  auto* clip = find_clip(inst.clips, modify_note.target);
  remove_note(inst, clip, src);

  Temporary<ClipNote, 1024> tmp_notes;
  Temporary<uint32_t, 1024> tmp_inds;
  auto tmp_note_view = tmp_notes.view();
  auto tmp_ind_view = tmp_inds.view();
  auto isecting = find_intersecting(inst, clip, dst, tmp_note_view, tmp_ind_view);

  remove_intersecting(inst, clip, isecting);
  insert_note(inst, clip, dst);
}

void modify_clip(Instance& inst, const NoteClipModification& mod) {
  auto& modify_clip = mod.modify_clip;
  auto* clip = find_clip(inst.clips, modify_clip.target);
  clip->span = modify_clip.span;
}

void remove_all_notes(Instance& inst, const NoteClipModification& mod) {
  auto* clip = find_clip(inst.clips, mod.remove_all_notes.target);
  remove_all_notes(inst, clip);
}

void apply_modification(Instance& inst, const NoteClipModification& mod) {
  using Type = NoteClipModification::Type;
  switch (mod.type) {
    case Type::CreateClip:
      create_clip(inst, mod);
      break;
    case Type::CloneClip:
      clone_clip(inst, mod);
      break;
    case Type::DestroyClip:
      destroy_clip(inst, mod);
      break;
    case Type::AddNote:
      add_note(inst, mod);
      break;
    case Type::RemoveNote:
      remove_note(inst, mod);
      break;
    case Type::ModifyNote:
      modify_note(inst, mod);
      break;
    case Type::ModifyClip:
      modify_clip(inst, mod);
      break;
    case Type::RemoveAllNotes:
      remove_all_notes(inst, mod);
      break;
    default:
      assert(false);
      break;
  }
}

[[maybe_unused]] bool is_valid_span(const ScoreRegion& reg) {
  const double num = reference_time_signature().numerator;
  return !reg.empty() &&
         reg.size.beat >= 0.0 && reg.size.beat < num &&
         reg.begin.beat >= 0.0 && reg.begin.beat < num;
}

} //  anon

NoteClipHandle ui_create_clip(NoteClipSystem* sys, const ScoreRegion& region) {
  NoteClipHandle result = next_clip_handle(sys);
  auto mod = make_create_clip_modification(region, result);
  create_clip(*sys->instance0, mod);
  push_modification(sys, mod);
  return result;
}

NoteClipHandle ui_clone_clip(NoteClipSystem* sys, NoteClipHandle clip) {
  NoteClipHandle result = next_clip_handle(sys);
  auto mod = make_clone_clip_modification(clip, result);
  clone_clip(*sys->instance0, mod);
  push_modification(sys, mod);
  return result;
}

const NoteClip* ui_read_clip(NoteClipSystem* sys, NoteClipHandle clip) {
  return find_clip(sys->instance0->clips, clip);
}

bool ui_is_clip(NoteClipSystem* sys, NoteClipHandle clip) {
  return ui_read_clip(sys, clip) != nullptr;
}

void ui_destroy_clip(NoteClipSystem* sys, NoteClipHandle clip) {
  auto mod = make_destroy_clip_modification(clip);
  destroy_clip(*sys->instance0, mod);
  push_modification(sys, mod);
}

void ui_add_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note) {
  assert(is_valid_note(note));
  auto mod = make_add_note_modification(clip, note);
  add_note(*sys->instance0, mod);
  push_modification(sys, mod);
}

void ui_remove_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note) {
  auto mod = make_remove_note_modification(clip, note);
  remove_note(*sys->instance0, mod);
  push_modification(sys, mod);
}

void ui_remove_existing_notes(
  NoteClipSystem* sys, NoteClipHandle clip, const ClipNote* notes, int num_notes) {
  //
  for (int i = 0; i < num_notes; i++) {
    if (ui_is_note(sys, clip, notes[i])) {
      ui_remove_note(sys, clip, notes[i]);
    }
  }
}

void ui_remove_all_notes(NoteClipSystem* sys, NoteClipHandle clip) {
  auto mod = make_remove_all_notes_modification(clip);
  remove_all_notes(*sys->instance0, mod);
  push_modification(sys, mod);
}

void ui_modify_note(
  NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& src, const ClipNote& dst) {
  //
  assert(is_valid_note(dst));
  auto mod = make_modify_note_modification(clip, src, dst);
  modify_note(*sys->instance0, mod);
  push_modification(sys, mod);
}

bool ui_is_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note) {
  auto* accel = &sys->instance0->note_accel;
  auto* tree = read_note_query_tree(accel, ui_read_clip(sys, clip)->note_accel_instance);
  auto* maybe_note = find_note(accel, tree, note.span.begin, note.note);
  return maybe_note != nullptr;
}

void ui_set_clip_span(NoteClipSystem* sys, NoteClipHandle clip, const ScoreRegion& span) {
  assert(is_valid_span(span));
  auto mod = make_modify_clip_modification(clip, span);
  modify_clip(*sys->instance0, mod);
  push_modification(sys, mod);
}

int render_collect_notes_starting_in_region(const NoteClipSystem* sys, const NoteClip* clip,
                                            const ScoreRegion& region, uint32_t* dst_indices,
                                            ClipNote* dst, int max_num) {
  return collect_notes_starting_in_region(
    &sys->render_instance->note_accel,
    read_note_query_tree(&sys->render_instance->note_accel, clip->note_accel_instance),
    region, dst_indices, dst, max_num);
}

const ClipNote* render_find_note(const NoteClipSystem* sys, const NoteClip* clip,
                                 ScoreCursor begin, ScoreCursor end, MIDINote search) {
  (void) end;
  auto* accel = &sys->render_instance->note_accel;
  auto* tree = render_read_note_query_tree(sys, clip);
  return find_note(accel, tree, begin, search);
}

const NoteClip* render_read_clip(const NoteClipSystem* sys, NoteClipHandle clip) {
  return find_clip(sys->render_instance->clips, clip);
}

const NoteQueryTree* render_read_note_query_tree(const NoteClipSystem* sys, const NoteClip* clip) {
  return read_note_query_tree(&sys->render_instance->note_accel, clip->note_accel_instance);
}

const ClipNote*
render_find_cursor_strictly_within_note(const NoteClipSystem* sys, const NoteQueryTree* accel,
                                        const ScoreCursor& cursor, MIDINote note) {
  return find_cursor_strictly_within_note(
    &sys->render_instance->note_accel, accel, cursor, note);
}

int ui_collect_notes_intersecting_region(const NoteClipSystem* sys, const NoteClip* clip,
                                         const ScoreRegion& region, uint32_t* dst_indices,
                                         ClipNote* dst, int max_num) {
  auto* inst = sys->instance0.get();
  return collect_notes_intersecting_region(
    &inst->note_accel,
    read_note_query_tree(&inst->note_accel, clip->note_accel_instance),
    region, dst_indices, dst, max_num);
}

namespace {

MIDINote c_relative_semitone_offset_to_midi_note(int st, int8_t base_octave, int8_t velocity) {
  const int8_t oct = int8_t(clamp(st / 12 + int(base_octave), -128, 127));
  auto pc = PitchClass(wrap_within_range(st, 12));
  MIDINote result{};
  result.octave = oct;
  result.pitch_class = pc;
  result.velocity = velocity;
  return result;
}

} //  anon

void ui_randomize_clip_contents(
  NoteClipSystem* clip_sys, NoteClipHandle clip_handle, ScoreCursor clip_size, double tsig_num,
  double p_rest, double beat_event_interval, const float* sts, int num_sts) {
  //
  ui_remove_all_notes(clip_sys, clip_handle);

  const double clip_size_beats = clip_size.to_beats(tsig_num);
  double event_isi = beat_event_interval;
  int num_events = std::max(1, int(clip_size_beats / event_isi));

  for (int e = 0; e < num_events; e++) {
    if (urand() < p_rest) {
      continue;
    }

    const float st = *uniform_array_sample(sts, num_sts);

    ScoreCursor start = ScoreCursor::from_beats(event_isi * double(e), tsig_num);
    ScoreCursor end = start;
    end.wrapped_add_beats(event_isi, tsig_num);

    ClipNote note{};
    note.span = ScoreRegion::from_begin_end(start, end, tsig_num);
    note.note = c_relative_semitone_offset_to_midi_note(int(st), 3, 127);
    ui_add_note(clip_sys, clip_handle, note);
  }
}

void initialize(NoteClipSystem* sys) {
  sys->instance0 = std::make_unique<Instance>();
  sys->instance1 = std::make_unique<Instance>();
  sys->instance2 = std::make_unique<Instance>();
  sys->render_instance = sys->instance2.get();
}

void ui_update(NoteClipSystem* sys) {
  if (sys->instance_handshake.awaiting_read && acknowledged(&sys->instance_handshake)) {
    for (auto& mod : sys->mods2) {
      apply_modification(*sys->instance2, mod);
    }
    std::swap(sys->instance1, sys->instance2);
    sys->mods2.clear();
  }

  if (!sys->mods1.empty() && !sys->instance_handshake.awaiting_read) {
    assert(sys->mods2.empty());
    for (auto& mod : sys->mods1) {
      apply_modification(*sys->instance1, mod);
      sys->mods2.push_back(mod);
    }
    publish(&sys->instance_handshake, sys->instance1.get());
    sys->mods1.clear();
  }
}

void begin_render(NoteClipSystem* sys) {
  if (auto res = read(&sys->instance_handshake)) {
    sys->render_instance = res.value();
  }
}

GROVE_NAMESPACE_END
