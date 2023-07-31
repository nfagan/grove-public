#pragma once

#include "NotePacketAllocator.hpp"
#include "NoteQueryAccelerator.hpp"
#include "grove/common/Handshake.hpp"

namespace grove {

struct NoteClipHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(NoteClipHandle, id)
  uint32_t id;
};

struct NoteClip {
  NoteClipHandle handle;
  NoteQueryAcceleratorInstanceHandle note_accel_instance;
  ScoreRegion span;
  ScoreCursor start_offset;
};

struct NoteClipModification {
public:
  enum class Type {
    CreateClip,
    CloneClip,
    DestroyClip,
    ModifyClip,
    AddNote,
    RemoveNote,
    ModifyNote,
    RemoveAllNotes
  };

  struct CreateClip {
    ScoreRegion region;
    NoteClipHandle handle;
  };

  struct DestroyClip {
    NoteClipHandle handle;
  };

  struct CloneClip {
    NoteClipHandle src;
    NoteClipHandle dst;
  };

  struct AddNote {
    NoteClipHandle target;
    ClipNote note;
  };

  struct RemoveNote {
    NoteClipHandle target;
    ClipNote note;
  };

  struct ModifyNote {
    NoteClipHandle target;
    ClipNote src;
    ClipNote dst;
  };

  struct ModifyClip {
    NoteClipHandle target;
    ScoreRegion span;
  };

  struct RemoveAllNotes {
    NoteClipHandle target;
  };

public:
  Type type;
  union {
    CreateClip create_clip;
    CloneClip clone_clip;
    DestroyClip destroy_clip;
    ModifyClip modify_clip;
    AddNote add_note;
    RemoveNote remove_note;
    ModifyNote modify_note;
    RemoveAllNotes remove_all_notes;
  };
};

struct NoteClipSystem {
  struct Instance {
    std::vector<NoteClip> clips;
    NoteQueryAccelerator note_accel;
  };

  std::unique_ptr<Instance> instance0;
  std::unique_ptr<Instance> instance1;
  std::unique_ptr<Instance> instance2;

  std::vector<NoteClipModification> mods1;
  std::vector<NoteClipModification> mods2;

  Instance* render_instance{};
  Handshake<Instance*> instance_handshake;

  uint32_t next_clip_id{1};
};

NoteClipHandle ui_create_clip(NoteClipSystem* sys, const ScoreRegion& region);
void ui_destroy_clip(NoteClipSystem* sys, NoteClipHandle clip);

NoteClipHandle ui_clone_clip(NoteClipSystem* sys, NoteClipHandle clip);
const NoteClip* ui_read_clip(NoteClipSystem* sys, NoteClipHandle clip);
bool ui_is_clip(NoteClipSystem* sys, NoteClipHandle clip);
bool ui_is_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note);
void ui_add_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note);
void ui_remove_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& note);
void ui_remove_existing_notes(
  NoteClipSystem* sys, NoteClipHandle clip, const ClipNote* notes, int num_notes);
void ui_remove_all_notes(NoteClipSystem* sys, NoteClipHandle clip);
void ui_modify_note(NoteClipSystem* sys, NoteClipHandle clip, const ClipNote& src, const ClipNote& dst);
void ui_set_clip_span(NoteClipSystem* sys, NoteClipHandle clip, const ScoreRegion& span);

int render_collect_notes_starting_in_region(const NoteClipSystem* sys, const NoteClip* clip,
                                            const ScoreRegion& region, uint32_t* dst_indices,
                                            ClipNote* dst, int max_num);
const ClipNote* render_find_note(const NoteClipSystem* sys, const NoteClip* clip,
                                 ScoreCursor begin, ScoreCursor end, MIDINote search);
const NoteClip* render_read_clip(const NoteClipSystem* sys, NoteClipHandle clip);
const ClipNote* render_find_cursor_strictly_within_note(const NoteClipSystem* sys,
                                                        const NoteQueryTree* accel,
                                                        const ScoreCursor& cursor, MIDINote note);
const NoteQueryTree* render_read_note_query_tree(const NoteClipSystem* sys, const NoteClip* clip);

int ui_collect_notes_intersecting_region(const NoteClipSystem* sys, const NoteClip* clip,
                                         const ScoreRegion& region, uint32_t* dst_indices,
                                         ClipNote* dst, int max_num);

void ui_randomize_clip_contents(
  NoteClipSystem* sys, NoteClipHandle clip, ScoreCursor clip_size, double tsig_num,
  double p_rest, double beat_event_interval, const float* sts, int num_sts);

void initialize(NoteClipSystem* sys);
void ui_update(NoteClipSystem* sys);
void begin_render(NoteClipSystem* sys);

}