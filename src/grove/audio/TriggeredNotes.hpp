#pragma once

#include "types.hpp"
#include "grove/common/Handshake.hpp"
#include "grove/common/ArrayView.hpp"
#include <unordered_set>

namespace grove {

struct MIDIMessageStreamSystem;
class Transport;

struct TriggeredNotes {
public:
  struct TrackNote {
    friend inline bool operator==(const TrackNote& a, const TrackNote& b) {
      return a.track == b.track &&
             a.note.pitch_class == b.note.pitch_class &&
             a.note.octave == b.note.octave;
    }

    uint32_t track;
    MIDINote note;
  };

  struct HashTrackNote {
    size_t operator()(const TrackNote& note) const noexcept {
      return std::hash<uint32_t>{}(note.track) ^ std::hash<int>{}(int(note.note.pitch_class));
    }
  };

  using TrackNoteSet = std::unordered_set<TrackNote, HashTrackNote>;

  struct PlayResult {
    bool is_ready;
    MIDINote note;
    ScoreRegion played_span;
  };

  struct Change {
  public:
    enum class Type {
      NoteOn,
      NoteOff
    };

  public:
    bool is_on() const {
      return type == Type::NoteOn;
    }
    bool is_off() const {
      return type == Type::NoteOff;
    }

  public:
    Type type;
    uint32_t track;
    MIDINote note;
    PlayResult* result;
    int64_t id;
  };

  struct PlayingNote {
    uint32_t track;
    MIDINote note;
    ScoreCursor began;
    double played_for_beats;
    int64_t id;
  };

  struct TimeoutNote {
    uint32_t track;
    MIDINote note;
    float remaining_s;
  };

  struct Instance {
    DynamicArray<Change, 32> changes;
    DynamicArray<PlayingNote, 32> playing_notes;
  };

  Instance instance0;
  Instance instance1;
  Instance instance2;

  DynamicArray<Change, 16> changes1;
  DynamicArray<Change, 16> changes2;

  TrackNoteSet pending_on;
  TrackNoteSet pending_off;

  DynamicArray<TimeoutNote, 32> timeout_notes;

  Instance* render_instance{};
  Instance* instance_ptr1{};
  Instance* instance_ptr2{};

  Handshake<Instance*> instance_handshake;
  int64_t next_id{1};

  const Transport* transport{};
  std::atomic<bool> initialized{};
  bool render_began_process{};
};

using TriggeredNoteChanges = ArrayView<const TriggeredNotes::Change>;

namespace notes {

TriggeredNotes* get_global_triggered_notes();
uint8_t get_triggered_notes_midi_source_id();

void ui_initialize(TriggeredNotes* notes, const Transport* transport);
bool ui_note_on(TriggeredNotes* notes, uint32_t track, MIDINote note);
bool ui_note_on_timeout(TriggeredNotes* notes, uint32_t track, MIDINote note, float seconds);
void ui_note_off(TriggeredNotes* notes, uint32_t track, MIDINote note);
bool ui_note_off(TriggeredNotes* notes, uint32_t track, MIDINote note,
                 TriggeredNotes::PlayResult* feedback);
void ui_flush_on(TriggeredNotes* notes, uint32_t track);
void ui_update(TriggeredNotes* notes, double real_dt);

TriggeredNoteChanges render_begin_process(TriggeredNotes* notes);
void render_push_messages_to_streams(MIDIMessageStreamSystem* sys, const TriggeredNoteChanges& changes);

void render_end_process(TriggeredNotes* notes);
bool render_is_playing_note(const TriggeredNotes* notes, uint32_t track, MIDINote note);

}

}