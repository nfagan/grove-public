#include "QuantizedTriggeredNotes.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "Transport.hpp"
#include "cursor.hpp"
#include "arpeggio.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/RingBuffer.hpp"
#include <vector>
#include <memory>

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr int num_tracks = 8;
  static constexpr int slots_per_track = 256;
  static constexpr double tsig_num = double(reference_time_signature().numerator);
  static constexpr uint8_t midi_message_stream_source_id = 3;
};

enum class NoteState {
  Inactive,
  PendingOn,
  On
};

struct NoteDescriptor {
  uint8_t note;
  audio::Quantization play_next_quantization;
  double play_for_beats;
};

struct RenderNoteFeedback {
  void mark_started_playing() {
    assert(!started_playing.load());
    started_playing.store(true);
  }

  std::atomic<bool> started_playing{};
};

struct UINoteMessage {
  NoteDescriptor note_desc;
  RenderNoteFeedback* note_feedback;
};

struct UIPendingFeedback {
  uint64_t id{};
  std::unique_ptr<RenderNoteFeedback> feedback;
};

struct RenderNoteInfo {
  bool is_on() const {
    return state == NoteState::On;
  }
  bool not_inactive() const {
    return state != NoteState::Inactive;
  }
  bool is_inactive() const {
    return state == NoteState::Inactive;
  }
  bool is_pending_on() const {
    return state == NoteState::PendingOn;
  }

  NoteDescriptor note_desc;
  NoteState state;
  ScoreCursor start;
  RenderNoteFeedback* feedback;
};

struct RenderNoteSlot {
  void pop_front_info() {
    infos[0] = infos[1];
    infos[1] = {};
  }

  RenderNoteInfo infos[2];
  RingBuffer<UINoteMessage, 3> pending_messages_from_ui;
};

NoteDescriptor make_note_desc(uint8_t note, audio::Quantization quant, double play_for_beats) {
  assert(play_for_beats > 0.0);
  NoteDescriptor result{};
  result.note = note;
  result.play_next_quantization = quant;
  result.play_for_beats = play_for_beats;
  return result;
}

double block_relative_sample(ScoreCursor loc, const ScoreRegion& block_region, double bps) {
  assert(block_region.contains(loc, Config::tsig_num));
  loc.wrapped_sub_cursor(block_region.begin, Config::tsig_num);
  return loc.to_sample_offset(1.0 / bps, Config::tsig_num);
}

bool quantum_table_entry(
  const ScoreRegion& block_region, audio::Quantization quant, double bps, int num_frames,
  int* dst_quant_frame, ScoreCursor* dst_quant_cursor) {
  //
  const auto next_loc = next_quantum(block_region.begin, quant, Config::tsig_num);

  if (block_region.begin == next_loc) {
    *dst_quant_frame = 0;
    *dst_quant_cursor = block_region.begin;
    return true;

  } else {
    assert(next_loc > block_region.begin);
    if (block_region.contains(next_loc, Config::tsig_num)) {
      //  Distance from current transport position to next quantized position.
      double frame_off = block_relative_sample(next_loc, block_region, bps);
      double frame = std::floor(frame_off);
      assert(frame >= 0 && frame < num_frames);
      *dst_quant_frame = std::min(int(frame), num_frames - 1);
      *dst_quant_cursor = next_loc;
      return true;
    }
  }

  return false;
}

uint32_t get_slot_index(uint32_t track, uint32_t note) {
  assert(track < uint32_t(Config::num_tracks));
  assert(note < uint32_t(Config::slots_per_track));
  return track * uint32_t(Config::slots_per_track) + note;
}

QuantizedTriggeredNoteMessage make_message(uint8_t track, uint8_t note, int frame, bool on) {
  QuantizedTriggeredNoteMessage result{};
  result.track = track;
  result.note = note;
  result.frame = frame;
  result.on = on;
  return result;
}

bool less_message(const QuantizedTriggeredNoteMessage& a, const QuantizedTriggeredNoteMessage& b) {
  return std::tie(a.track, a.frame, a.on, a.note) < std::tie(b.track, b.frame, b.on, b.note);
}

} //  anon

struct QuantizedTriggeredNotes {
  void render_push_message(QuantizedTriggeredNoteMessage message) {
    note_messages.emplace_back() = message;
  }

  void render_finish_note_messages() {
    std::sort(note_messages.begin(), note_messages.end(), less_message);
    uint32_t curr_max = max_num_note_messages.load();
    auto new_max = std::max(curr_max, uint32_t(note_messages.size()));
    max_num_note_messages.store(new_max);
  }

  void return_note_feedback(std::unique_ptr<RenderNoteFeedback> feedback) {
    feedback->started_playing.store(false);
    store_feedbacks.push_back(std::move(feedback));
  }

  std::unique_ptr<RenderNoteFeedback> require_note_feedback() {
    if (store_feedbacks.empty()) {
      num_note_feedbacks_created++;
      return std::make_unique<RenderNoteFeedback>();
    } else {
      auto res = std::move(store_feedbacks.back());
      store_feedbacks.pop_back();
      return res;
    }
  }

  uint64_t next_feedback_id() {
    return feedback_id++;
  }

  uint32_t get_max_num_note_messages() const {
    return max_num_note_messages.load();
  }

  std::atomic<bool> ui_initialized{};
  const Transport* transport{};

  bool render_initialized{};
  RenderNoteSlot note_slots[Config::num_tracks * Config::slots_per_track]{};
  std::vector<QuantizedTriggeredNoteMessage> note_messages;

  std::vector<std::unique_ptr<RenderNoteFeedback>> store_feedbacks;
  std::vector<UIPendingFeedback> ui_pending_feedback;
  std::vector<uint64_t> newly_played;

  std::atomic<uint32_t> max_num_note_messages{};
  int num_note_feedbacks_created{};

  uint64_t feedback_id{1};
};

namespace {

struct {
  QuantizedTriggeredNotes notes;
} globals;

} //  anon

QuantizedTriggeredNotes* qtn::get_global_quantized_triggered_notes() {
  return &globals.notes;
}

void qtn::ui_initialize(QuantizedTriggeredNotes* notes, const Transport* transport) {
  assert(!notes->ui_initialized.load());
  notes->transport = transport;
  notes->ui_initialized.store(true);
}

uint64_t qtn::ui_trigger(
  QuantizedTriggeredNotes* notes, uint32_t track, MIDINote note,
  audio::Quantization next_quantum, double beat_duration) {
  //
  if (beat_duration <= 0.0) {
    return 0;
  }

  const uint32_t si = get_slot_index(track, note.note_number());
  auto& slot = notes->note_slots[si];
  if (slot.pending_messages_from_ui.full()) {
    return 0;
  }

  const uint64_t next_id = notes->next_feedback_id();

  auto& pend = notes->ui_pending_feedback.emplace_back();
  pend.feedback = notes->require_note_feedback();
  pend.id = next_id;

  UINoteMessage message{};
  message.note_desc = make_note_desc(note.note_number(), next_quantum, beat_duration);
  message.note_feedback = pend.feedback.get();
  slot.pending_messages_from_ui.write(message);
  return next_id;
}

QuantizedTriggeredNotesUpdateResult qtn::ui_update(QuantizedTriggeredNotes* notes) {
  QuantizedTriggeredNotesUpdateResult result{};

  notes->newly_played.clear();

  auto pend_it = notes->ui_pending_feedback.begin();
  while (pend_it != notes->ui_pending_feedback.end()) {
    UIPendingFeedback& pend = *pend_it;
    if (pend.feedback->started_playing.load()) {
      notes->newly_played.push_back(pend.id);
      notes->return_note_feedback(std::move(pend.feedback));
      pend_it = notes->ui_pending_feedback.erase(pend_it);
    } else {
      ++pend_it;
    }
  }

  result.newly_played = make_view(notes->newly_played);
  return result;
}

QuantizedTriggeredNotesStats qtn::ui_get_stats(const QuantizedTriggeredNotes* notes) {
  QuantizedTriggeredNotesStats result{};
  result.num_ui_pending_feedback = int(notes->ui_pending_feedback.size());
  result.max_num_note_messages = int(notes->get_max_num_note_messages());
  result.num_note_feedbacks_created = notes->num_note_feedbacks_created;
  return result;
}

uint8_t qtn::ui_get_midi_source_id() {
  return Config::midi_message_stream_source_id;
}

void qtn::begin_process(
  QuantizedTriggeredNotes* notes, MIDIMessageStreamSystem* midi_message_stream_sys,
  const AudioRenderInfo& info) {
  //
  if (!notes->ui_initialized.load()) {
    return;
  }

  if (!notes->render_initialized) {
    notes->note_messages.reserve(1024);
    notes->render_initialized = true;
  }

  notes->note_messages.clear();

  const double bps = beats_per_sample_at_bpm(
    notes->transport->get_bpm(), info.sample_rate, reference_time_signature());
  const ScoreRegion block_region{
    notes->transport->render_get_pausing_cursor_location(),
    ScoreCursor::from_beats(bps * info.num_frames, Config::tsig_num)};

  //  init quantization cursor / frame table.
  constexpr int num_quants = 7;
  int quant_frames[num_quants];
  ScoreCursor quant_cursors[num_quants];
  //  for each quantization, quant_frames[i] is -1 if the start of the quantum does not lie within
  //  the render epoch.
  std::fill(quant_frames, quant_frames + num_quants, -1);
  for (int i = 0; i < num_quants; i++) {
    (void) quantum_table_entry(
      block_region, audio::Quantization(i), bps, info.num_frames,
      &quant_frames[i], &quant_cursors[i]);
  }

  const bool just_played = notes->transport->just_played();

  //  For each slot ...
  for (int si = 0; si < Config::num_tracks * Config::slots_per_track; si++) {
    auto& render_slot = notes->note_slots[si];
    const auto track = uint8_t(si / Config::slots_per_track);

    if (render_slot.pending_messages_from_ui.size() > 0 && render_slot.infos[1].is_inactive()) {
      const auto note = render_slot.pending_messages_from_ui.read();
      int i = 0;
      if (render_slot.infos[0].not_inactive()) {
        //  Waiting on the first slot to finish.
        i = 1;
      }
      assert(note.note_feedback);
      render_slot.infos[i].state = NoteState::PendingOn;
      render_slot.infos[i].note_desc = note.note_desc;
      render_slot.infos[i].feedback = note.note_feedback;
    }

    Optional<ScoreCursor> prev_end;
    int prev_end_frame{-1};
    for (int it = 0; it < 2; it++) {
      //  @NOTE: do evaluate the 0th slot in both iterations.
      auto& info0 = render_slot.infos[0];

      if (just_played && info0.is_on()) {
        notes->render_push_message(make_message(track, info0.note_desc.note, 0, false));
        render_slot.pop_front_info();
        continue;
      }

      if (info0.is_pending_on()) {
        assert(info0.start == ScoreCursor{});

        Optional<ScoreCursor> quant_beg;
        Optional<int> quant_frame;
        if (prev_end) {
          //  Can only play at the nearest quantum following the end of the previously-on note.
          auto loc = next_quantum(
            prev_end.value(), info0.note_desc.play_next_quantization, Config::tsig_num);
          if (block_region.contains(loc, Config::tsig_num)) {
            double frame = block_relative_sample(loc, block_region, bps);
            assert(frame >= 0.0 && frame <= info.num_frames);
            int dst_frame = std::min(int(std::floor(frame)), info.num_frames - 1);
            //  @NOTE: Prefer to issue the previous note-off event first in that case.
            quant_frame = std::max(dst_frame, prev_end_frame);
            quant_beg = loc;
          }
        } else {
          const auto play_quant = int(info0.note_desc.play_next_quantization);
          assert(play_quant >= 0 && play_quant < num_quants);
          const int candidate_frame = quant_frames[play_quant];
          if (candidate_frame >= 0) {
            quant_frame = candidate_frame;
            quant_beg = quant_cursors[play_quant];
          }
        }

        if (quant_frame) {
          assert(quant_beg && block_region.contains(quant_beg.value(), Config::tsig_num));
          assert(quant_frame.value() >= 0 && quant_frame.value() < info.num_frames);
          //  Note on.
          info0.start = quant_beg.value();
          info0.state = NoteState::On;

          //  @NOTE: Push note on event.
          const int start_frame = quant_frame.value();
          notes->render_push_message(make_message(track, info0.note_desc.note, start_frame, true));

          //  feedback to ui.
          info0.feedback->mark_started_playing();
        }
      }

      if (info0.is_on()) {
        auto end = info0.start;
        end.wrapped_add_beats(info0.note_desc.play_for_beats, Config::tsig_num);

        if (block_region.contains(end, Config::tsig_num)) {
          const double frame = block_relative_sample(end, block_region, bps);
          assert(frame >= 0.0 && frame <= info.num_frames);
          int stop_frame = std::min(int(std::floor(frame)), info.num_frames - 1);

          prev_end = end;
          prev_end_frame = stop_frame;

          //  @NOTE: Push note off event.
          notes->render_push_message(make_message(track, info0.note_desc.note, stop_frame, false));

          //  advance to next slot.
          render_slot.pop_front_info();
        }
      }
    }
  }

  notes->render_finish_note_messages();
#ifdef GROVE_DEBUG
  for (auto& message : notes->note_messages) {
    assert(message.frame >= 0 && message.frame < info.num_frames);
    assert(int(message.track) < Config::num_tracks);
  }
#endif

  for (auto& message : notes->note_messages) {
    MIDIStreamMessage msg{};
    msg.source_id = Config::midi_message_stream_source_id;
    msg.frame = message.frame;
    msg.message = message.on ?
      MIDIMessage::make_note_on(0, message.note, 127) :
      MIDIMessage::make_note_off(0, message.note, 127);
    midi::render_broadcast_messages(midi_message_stream_sys, &msg, 1);
  }
}

GROVE_NAMESPACE_END
