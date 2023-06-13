#include "TriggeredNotes.hpp"
#include "Transport.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr uint8_t midi_message_source_id = 2;
};

using PlayingNote = TriggeredNotes::PlayingNote;
using Instance = TriggeredNotes::Instance;
using Change = TriggeredNotes::Change;
using TrackNote = TriggeredNotes::TrackNote;

TrackNote make_track_note(uint32_t track, MIDINote note) {
  TrackNote result{};
  result.track = track;
  result.note = note;
  return result;
}

bool less_by_track(const PlayingNote& a, const PlayingNote& b) {
  return a.track < b.track;
}

[[maybe_unused]] bool is_sorted(const PlayingNote* begin, const PlayingNote* end) {
  return std::is_sorted(begin, end, less_by_track);
}

[[maybe_unused]] void validate_playing_notes(const Instance& inst) {
  assert(is_sorted(inst.playing_notes.begin(), inst.playing_notes.end()));
  (void) inst;
}

auto find_change(const Change* begin, const Change* end, const TrackNote& note) {
  for (; begin != end; ++begin) {
    if (begin->track == note.track &&
        begin->note.matches_pitch_class_and_octave(note.note)) {
      return begin;
    }
  }
  return end;
}

template <typename Note>
Note* find_track(uint32_t track, Note* notes, int num_notes) {
  PlayingNote search{};
  search.track = track;
  auto it = std::lower_bound(notes, notes + num_notes, search, less_by_track);
  if (it == notes + num_notes || it->track != track) {
    return nullptr;
  } else {
    return it;
  }
}

template <typename Note>
Note* find_track_end(Note* track_begin, Note* end) {
  auto track = track_begin->track;
  for (; track_begin != end && track_begin->track == track; ++track_begin) {}
  return track_begin;
}

template <typename Note>
Note* find_playing_note(uint32_t track, MIDINote note, Note* notes, int num_notes) {
  if (auto* it = find_track(track, notes, num_notes)) {
    for (; it != notes + num_notes && it->track == track; ++it) {
      if (it->note.matches_pitch_class_and_octave(note)) {
        return it;
      }
    }
  }
  return nullptr;
}

template <typename Note>
Note* find_playing_note(int64_t id, Note* begin, Note* end) {
  return std::find_if(begin, end, [id](Note& src) {
    return src.id == id;
  });
}

PlayingNote* find_playing_note(uint32_t track, MIDINote note, Instance& instance) {
  auto* notes = instance.playing_notes.data();
  const auto num_notes = int(instance.playing_notes.size());
  return find_playing_note(track, note, notes, num_notes);
}

bool is_playing_note(uint32_t track, MIDINote note, const PlayingNote* notes, int num_notes) {
  return find_playing_note(track, note, notes, num_notes) != nullptr;
}

bool ui_is_playing_note(const TriggeredNotes* notes, uint32_t track, MIDINote note) {
  auto& playing = notes->instance0.playing_notes;
  const auto num_playing = int(playing.size());
  return is_playing_note(track, note, playing.data(), num_playing);
}

bool ui_is_pending_on(const TriggeredNotes* notes, uint32_t track, MIDINote note) {
  return notes->pending_on.count(make_track_note(track, note));
}

bool ui_is_pending_off(const TriggeredNotes* notes, uint32_t track, MIDINote note) {
  return notes->pending_off.count(make_track_note(track, note));
}

Change make_note_on_change(uint32_t track, MIDINote note, int64_t id) {
  Change change{};
  change.type = TriggeredNotes::Change::Type::NoteOn;
  change.track = track;
  change.note = note;
  change.id = id;
  return change;
}

Change make_note_off_change(uint32_t track, MIDINote note, TriggeredNotes::PlayResult* res,
                            int64_t id) {
  Change change{};
  change.type = TriggeredNotes::Change::Type::NoteOff;
  change.track = track;
  change.note = note;
  change.result = res;
  change.id = id;
  return change;
}

void note_on(Instance& inst, const Change& change) {
  PlayingNote note{};
  note.note = change.note;
  note.track = change.track;
  note.id = change.id;
  inst.playing_notes.push_back(note);
  std::sort(inst.playing_notes.begin(), inst.playing_notes.end(), less_by_track);
}

void note_off(Instance& inst, uint32_t track, MIDINote note) {
  auto& playing = inst.playing_notes;
  const auto num_playing = int(playing.size());
  auto it = find_playing_note(track, note, playing.data(), num_playing);
  assert(it);
  playing.erase(it);
}

void apply_change(Instance& inst, const Change& change) {
  switch (change.type) {
    case Change::Type::NoteOn:
      note_on(inst, change);
      return;
    case Change::Type::NoteOff:
      note_off(inst, change.track, change.note);
      return;
    default: {
      assert(false);
    }
  }
}

void ui_abort_pending_on(TriggeredNotes* notes, uint32_t track, MIDINote note) {
  note_off(notes->instance0, track, note);

  auto track_note = make_track_note(track, note);
  auto change_it = find_change(notes->changes1.begin(), notes->changes1.end(), track_note);
  assert(change_it != notes->changes1.end());
  notes->changes1.erase(change_it);
  notes->pending_on.erase(track_note);
}

void on_ack(TriggeredNotes* notes) {
  for (auto& change : notes->changes2) {
    if (change.is_off() && change.result) {
      auto* playing = find_playing_note(change.track, change.note, *notes->instance_ptr2);
      assert(playing);
      change.result->note = change.note;
      change.result->played_span.begin = playing->began;
      change.result->played_span.size = ScoreCursor::from_beats(
        playing->played_for_beats,
        reference_time_signature().numerator);
      change.result->is_ready = true;
    }
    apply_change(*notes->instance_ptr2, change);
  }
  notes->changes2.clear();
  notes->instance_ptr2->changes.clear();
  std::swap(notes->instance_ptr2, notes->instance_ptr1);
}

void on_change(TriggeredNotes* notes) {
  assert(notes->instance_ptr1->changes.empty() && notes->changes2.empty());
  for (auto& change : notes->changes1) {
    apply_change(*notes->instance_ptr1, change);
    notes->instance_ptr1->changes.push_back(change);
    notes->changes2.push_back(change);
  }
  notes->changes1.clear();
  notes->pending_on.clear();
  notes->pending_off.clear();
  publish(&notes->instance_handshake, std::move(notes->instance_ptr1));
}

void update_timeout_notes(TriggeredNotes* notes, float dt) {
  int num_notes = int(notes->timeout_notes.size());
  int ni{};
  for (int i = 0; i < num_notes; i++) {
    auto& note = notes->timeout_notes[ni];
    note.remaining_s -= dt;
    if (note.remaining_s <= 0.0f) {
      notes::ui_note_off(notes, note.track, note.note);
      notes->timeout_notes.erase(notes->timeout_notes.begin() + ni);
    } else {
      ++ni;
    }
  }
}

void render_start_playing(Instance& inst, ScoreCursor transport_cursor) {
  for (auto& change : inst.changes) {
    if (change.is_on()) {
      auto* playing = find_playing_note(change.track, change.note, inst);
      assert(playing && playing->began == ScoreCursor{} && playing->played_for_beats == 0.0);
      playing->began = transport_cursor;
      playing->played_for_beats = 0.0;
    }
  }
}

void render_copy_play_status(Instance& curr_inst, Instance& new_inst) {
  const auto num_curr = int(curr_inst.playing_notes.size());
  auto* curr_playing = curr_inst.playing_notes.data();
  auto* src_curr_end = curr_inst.playing_notes.data() + num_curr;

  const auto num_new = int(new_inst.playing_notes.size());
  auto* new_playing = new_inst.playing_notes.data();
  auto* src_new = new_playing;
  auto* src_new_end = src_new + num_new;

  while (curr_playing != src_curr_end) {
    auto* curr_end = find_track_end(curr_playing, src_curr_end);
    if (auto* np = find_track(curr_playing->track, new_playing, int(src_new_end - new_playing))) {
      new_playing = np;
      assert(new_playing->track == curr_playing->track);
      auto* new_end = find_track_end(new_playing, src_new_end);
      for (; curr_playing != curr_end; ++curr_playing) {
        auto* new_note = find_playing_note(curr_playing->id, new_playing, new_end);
        if (new_note != new_end) {
          assert(new_note->track == curr_playing->track && new_note->id == curr_playing->id);
          new_note->played_for_beats = curr_playing->played_for_beats;
          new_note->began = curr_playing->began;
        }
      }
      new_playing = new_end;
    }
    curr_playing = curr_end;
  }
}

struct {
  TriggeredNotes notes;
} globals;

} //  anon

TriggeredNotes* notes::get_global_triggered_notes() {
  return &globals.notes;
}

uint8_t notes::get_triggered_notes_midi_source_id() {
  return Config::midi_message_source_id;
}

bool notes::ui_note_on(TriggeredNotes* notes, uint32_t track, MIDINote note) {
  if (ui_is_playing_note(notes, track, note) || ui_is_pending_off(notes, track, note)) {
    return false;
  }

  assert(!notes->pending_on.count(make_track_note(track, note)));
  auto change = make_note_on_change(track, note, notes->next_id++);
  apply_change(notes->instance0, change);
  notes->changes1.push_back(change);
  notes->pending_on.insert(make_track_note(track, note));
#ifdef GROVE_DEBUG
  validate_playing_notes(notes->instance0);
#endif
  return true;
}

bool notes::ui_note_on_timeout(TriggeredNotes* notes, uint32_t track, MIDINote note, float seconds) {
  if (ui_note_on(notes, track, note)) {
    TriggeredNotes::TimeoutNote timeout_note{};
    timeout_note.track = track;
    timeout_note.note = note;
    timeout_note.remaining_s = seconds;
    notes->timeout_notes.push_back(timeout_note);
    return true;
  } else {
    return false;
  }
}

void notes::ui_note_off(TriggeredNotes* notes, uint32_t track, MIDINote note) {
  (void) ui_note_off(notes, track, note, nullptr);
}

bool notes::ui_note_off(
  TriggeredNotes* notes, uint32_t track, MIDINote note, TriggeredNotes::PlayResult* request_result) {
  //
  if (!ui_is_playing_note(notes, track, note)) {
    return false;
  }

  if (ui_is_pending_on(notes, track, note)) {
    ui_abort_pending_on(notes, track, note);
    if (request_result) {
      request_result->played_span = {};
      request_result->note = note;
      request_result->is_ready = true;
    }
  } else {
    auto change = make_note_off_change(track, note, request_result, notes->next_id++);
    apply_change(notes->instance0, change);
    notes->changes1.push_back(change);
    assert(!notes->pending_off.count(make_track_note(track, note)));
    notes->pending_off.insert(make_track_note(track, note));
  }
#ifdef GROVE_DEBUG
  validate_playing_notes(notes->instance0);
#endif
  return true;
}

void notes::ui_flush_on(TriggeredNotes* notes, uint32_t track_id) {
  auto& playing = notes->instance0.playing_notes;
  auto* track = find_track(track_id, playing.data(), int(playing.size()));
  if (!track) {
    return;
  }
  auto track_end = find_track_end(track, playing.end());

  Temporary<Change, 64> tmp_change_store;
  Change* changes = tmp_change_store.require(int(track_end - track));

  int ind{};
  for (; track != track_end; ++track) {
    assert(track->track == track_id);
    changes[ind++] = make_note_off_change(track_id, track->note, nullptr, notes->next_id++);
  }

  for (int i = 0; i < ind; i++) {
    auto note = changes[i].note;
    if (ui_is_pending_on(notes, track_id, note)) {
      ui_abort_pending_on(notes, track_id, note);
    } else {
      assert(!notes->pending_off.count(make_track_note(track_id, note)));
      apply_change(notes->instance0, changes[i]);
      notes->changes1.push_back(changes[i]);
      notes->pending_off.insert(make_track_note(track_id, note));
    }
  }
#ifdef GROVE_DEBUG
  validate_playing_notes(notes->instance0);
#endif
}

void notes::ui_initialize(TriggeredNotes* notes, const Transport* transport) {
  assert(!notes->initialized.load());
  notes->render_instance = &notes->instance2;
  notes->instance_ptr1 = &notes->instance1;
  notes->instance_ptr2 = &notes->instance2;
  notes->transport = transport;
  notes->initialized.store(true);
}

void notes::ui_update(TriggeredNotes* notes, double real_dt) {
  assert(notes->changes1.size() == int64_t(notes->pending_off.size() + notes->pending_on.size()));
  if (notes->instance_handshake.awaiting_read && acknowledged(&notes->instance_handshake)) {
    on_ack(notes);
  }
  if (!notes->instance_handshake.awaiting_read && !notes->changes1.empty()) {
    on_change(notes);
  }

  update_timeout_notes(notes, float(real_dt));
}

void notes::render_push_messages_to_streams(
  MIDIMessageStreamSystem* sys, const TriggeredNoteChanges& changes) {
  //  @TODO: We don't have a way of specifying the MIDI channel when generating notes right now.
  for (auto& change : changes) {
    MIDIStreamMessage msg{};
    if (change.is_on()) {
      msg.message = MIDIMessage::make_note_on(0, change.note);
    } else {
      msg.message = MIDIMessage::make_note_off(0, change.note);
    }
    msg.frame = 0;
    msg.source_id = Config::midi_message_source_id;
    (void) midi::render_push_messages(sys, MIDIMessageStreamHandle{change.track}, &msg, 1);
  }
}

TriggeredNoteChanges notes::render_begin_process(TriggeredNotes* notes) {
  notes->render_began_process = false;

  if (!notes->initialized.load()) {
    return {};
  } else {
    notes->render_began_process = true;
  }

  auto* inst_ptr = peek(&notes->instance_handshake);
  if (!inst_ptr) {
    return {};
  }

#ifdef GROVE_DEBUG
  validate_playing_notes(**inst_ptr);
#endif
  render_copy_play_status(*notes->render_instance, **inst_ptr);
  auto inst = read(&notes->instance_handshake);
  notes->render_instance = inst.unwrap();
  render_start_playing(*notes->render_instance, notes->transport->render_get_cursor_location());
#ifdef GROVE_DEBUG
  validate_playing_notes(*notes->render_instance);
#endif
  return make_view(notes->render_instance->changes);
}

void notes::render_end_process(TriggeredNotes* notes) {
  if (!notes->render_began_process) {
    return;
  }

  auto block_size = notes->transport->render_get_process_block_size();
  auto elapsed = block_size.to_beats(reference_time_signature().numerator);
  for (auto& note : notes->render_instance->playing_notes) {
    note.played_for_beats += elapsed;
  }
}

bool notes::render_is_playing_note(const TriggeredNotes* notes, uint32_t track, MIDINote note) {
  if (!notes->render_began_process) {
    return false;
  }

  auto& playing = notes->render_instance->playing_notes;
  return is_playing_note(track, note, playing.data(), int(playing.size()));
}

GROVE_NAMESPACE_END
