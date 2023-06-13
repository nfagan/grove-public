#include "NoteClipStateMachineSystem.hpp"
#include "arpeggio.hpp"
#include "types.hpp"
#include "cursor.hpp"
#include "Transport.hpp"
#include "NoteClipSystem.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include "grove/common/RingBuffer.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Stopwatch.hpp"

GROVE_NAMESPACE_BEGIN

struct Config {
  static constexpr int max_num_voices = 32;
  static constexpr int max_num_sections = 64;
  static constexpr uint8_t midi_source_id = 6;
};

struct VoiceFeedbackEntry {
  int section;
  Optional<int> next_section;
  int num_section_repetitions;
  ScoreCursor position;
  ScoreCursor elapsed;
};

struct VoiceFeedbackToUI {
  VoiceFeedbackEntry entries[Config::max_num_voices];
};

struct RenderBlockInfo {
  int num_frames;
  double spb;
  double bps;
  double tsig_num;
  ScoreCursor block_size;
};

struct PlayingNote {
  ClipNote clip_note;
  bool just_began;
};

struct PlayingNotes {
  void push(PlayingNote note) {
    playing.push_back(note);
    assert(playing.size() <= 256);
  }

  DynamicArray<PlayingNote, 256> playing;
};

struct RenderVoice {
  ScoreCursor elapsed{};
  int section_index{};
  Optional<int> next_section_index;
  int num_section_repetitions{};
  PlayingNotes playing_notes;
  Optional<MIDIMessageStreamHandle> midi_stream;
};

struct UIVoice {
  bool acquired{};
  int section_index{};
  Optional<int> pending_next_section_index;
  ScoreCursor approximate_cursor_position{};
  ScoreCursor approximate_elapsed{};
  Optional<int> approximate_next_section_index;
  int approximate_num_section_repetitions;
  Handshake<int> handoff_next_section_index;
  std::atomic<uint32_t> midi_message_stream_id{};
};

struct RenderSection {
  NoteClipHandle clip_handle{};
  const NoteClip* clip{};
};

struct UISection {
  NoteClipHandle clip_handle{};
};

struct NoteClipStateMachineSystem {
  std::atomic<bool> initialized{};

  const Transport* transport{};
  NoteClipSystem* clip_system{};
  MIDIMessageStreamSystem* midi_stream_system{};

  RenderVoice render_voices[Config::max_num_voices]{};
  UIVoice ui_voices[Config::max_num_voices]{};
  int num_voices{};

  RenderSection render_sections[Config::max_num_sections]{};
  UISection ui_sections[Config::max_num_sections]{};
  int num_sections{};

  RingBuffer<VoiceFeedbackToUI, 2> voice_feedback_to_ui;
};

namespace {

int to_frame_offset(const ScoreCursor& beg, const RenderBlockInfo& block_info) {
  int frame = int(beg.to_sample_offset(block_info.spb, block_info.tsig_num));
  assert(frame >= 0 && frame < block_info.num_frames);
  frame = clamp(frame, 0, block_info.num_frames - 1);
  return frame;
}

RenderBlockInfo make_block_info(const Transport* transport, const AudioRenderInfo& info) {
  double tsig_num = reference_time_signature().numerator;
  RenderBlockInfo result{};
  result.tsig_num = tsig_num;
  result.bps = transport->render_get_beats_per_sample(info.sample_rate);
  result.spb = 1.0 / result.bps;
  result.num_frames = info.num_frames;
  result.block_size = ScoreCursor::from_beats(result.bps * double(info.num_frames), tsig_num);
  return result;
}

void start_playing_notes(
  NoteClipStateMachineSystem* sys, const NoteClip* clip, RenderVoice& voice,
  const ScoreRegion& span, const ScoreCursor& off, const RenderBlockInfo& block_info) {
  //
  constexpr int note_stack_size = 1024;
  Temporary<uint32_t, note_stack_size> note_indices_store;
  Temporary<ClipNote, note_stack_size> note_store;
  uint32_t* note_indices = note_indices_store.require(note_stack_size);
  ClipNote* notes = note_store.require(note_stack_size);

  int num_req = render_collect_notes_starting_in_region(
    sys->clip_system, clip, span, note_indices, notes, note_stack_size);

  if (num_req > note_stack_size) {
    assert(false);
    note_indices = note_indices_store.require(num_req);
    notes = note_store.require(num_req);
    (void) render_collect_notes_starting_in_region(
      sys->clip_system, clip, span, note_indices, notes, note_stack_size);
  }

  for (int i = 0; i < num_req; i++) {
    auto beg = notes[i].span.begin;
    assert(beg >= span.begin);
    beg.wrapped_sub_cursor(span.begin, block_info.tsig_num);
    beg.wrapped_add_cursor(off, block_info.tsig_num);

    PlayingNote playing{};
    playing.clip_note = notes[i];
    playing.just_began = true;
    voice.playing_notes.push(playing);

    if (voice.midi_stream) {
      MIDIStreamMessage message{};
      message.source_id = Config::midi_source_id;
      message.frame = to_frame_offset(beg, block_info);
      message.message = MIDIMessage::make_note_on(0, notes[i].note);
      (void) midi::render_push_messages(
        sys->midi_stream_system, voice.midi_stream.value(), &message, 1);
    }
  }
}

void stop_playing_notes(
  NoteClipStateMachineSystem* sys, const NoteClip* clip, RenderVoice& voice,
  const ScoreRegion& span, const ScoreCursor& off, const RenderBlockInfo& block_info) {
  //
  const double tsig_num = block_info.tsig_num;
  const ScoreCursor span_end = span.end(tsig_num);

  int ni{};
  int np = int(voice.playing_notes.playing.size());
  for (int i = 0; i < np; i++) {
    auto& playing_note = voice.playing_notes.playing[ni];
    const bool just_began = playing_note.just_began;
    playing_note.just_began = false;

    ScoreCursor note_end = playing_note.clip_note.span.end(block_info.tsig_num);
    const ClipNote* query_note = render_find_note(
      sys->clip_system, clip, playing_note.clip_note.span.begin, note_end, playing_note.clip_note.note);

    Optional<int> stop_at;
    if (!query_note) {
      //  stop playing immediately.
      stop_at = to_frame_offset(off, block_info);
    } else {
      note_end = clip->span.loop(
        std::min(clip->span.end(tsig_num), query_note->span.end(tsig_num)), tsig_num);
      if (note_end >= span.begin && note_end < span_end) {
        if (!just_began || note_end != playing_note.clip_note.span.begin) {
          //  stop playing at `(note_end - seg_beg) + off` frames.
          note_end.wrapped_sub_cursor(span.begin, tsig_num);
          note_end.wrapped_add_cursor(off, tsig_num);
          stop_at = to_frame_offset(note_end, block_info);
        }
      }
    }

    if (stop_at) {
      if (voice.midi_stream) {
        MIDIStreamMessage message{};
        message.source_id = Config::midi_source_id;
        message.frame = stop_at.value();
        message.message = MIDIMessage::make_note_off(0, playing_note.clip_note.note);
        (void) midi::render_push_messages(
          sys->midi_stream_system, voice.midi_stream.value(), &message, 1);
      }
      voice.playing_notes.playing.erase(voice.playing_notes.playing.begin() + ni);
    } else {
      ++ni;
    }
  }
}

void clear_playing_notes(NoteClipStateMachineSystem* sys, RenderVoice& voice) {
  if (voice.midi_stream) {
    for (auto& note : voice.playing_notes.playing) {
      MIDIStreamMessage message{};
      message.source_id = Config::midi_source_id;
      message.frame = 0;
      message.message = MIDIMessage::make_note_off(0, note.clip_note.note);
      (void) midi::render_push_messages(
        sys->midi_stream_system, voice.midi_stream.value(), &message, 1);
    }
  }
  voice.playing_notes.playing.clear();
}

bool check_can_begin_process(NoteClipStateMachineSystem* sys, const AudioRenderInfo&) {
  if (!sys->initialized.load() || sys->num_sections == 0 || sys->num_voices == 0) {
    return false;
  }

  for (int i = 0; i < sys->num_sections; i++) {
    auto& section = sys->render_sections[i];
    section.clip = render_read_clip(sys->clip_system, section.clip_handle);
    if (!section.clip) {
      return false;
    } else {
      assert(!section.clip->span.empty());
      assert(section.clip->span.begin == ScoreCursor{});
#ifdef GROVE_DEBUG
      auto enc_size = decode(encode(section.clip->span.size, QuantizedScoreCursor::Depth::D16));
      assert(enc_size == section.clip->span.size);
#endif
    }
  }

  return true;
}

void maybe_advance_to_next_section(NoteClipStateMachineSystem* sys, RenderVoice& voice) {
  if (voice.next_section_index) {
    const int curr_si = voice.section_index;
    assert(voice.next_section_index.value() >= 0);
    voice.section_index = voice.next_section_index.value() % sys->num_sections;
    voice.next_section_index = NullOpt{};
    if (curr_si != voice.section_index) {
      voice.num_section_repetitions = 0;
    }
  }
}

void begin_process(NoteClipStateMachineSystem* sys, const AudioRenderInfo& info) {
  if (!check_can_begin_process(sys, info)) {
    return;
  }

  for (int v = 0; v < sys->num_voices; v++) {
    {
      const uint32_t stream_id = sys->ui_voices[v].midi_message_stream_id.load();
      MIDIMessageStreamHandle stream_handle{stream_id};
      if (midi::render_can_write_to_stream(sys->midi_stream_system, stream_handle)) {
        sys->render_voices[v].midi_stream = stream_handle;
      } else {
        sys->render_voices[v].midi_stream = NullOpt{};
      }
    }
    if (auto ind = read(&sys->ui_voices[v].handoff_next_section_index)) {
      sys->render_voices[v].next_section_index = ind.value();
    }
  }

  auto* transport = sys->transport;
  if (transport->just_stopped()) {
    for (int v = 0; v < sys->num_voices; v++) {
      auto& voice = sys->render_voices[v];
      voice.elapsed = {};
      clear_playing_notes(sys, voice);
    }
  } else if (transport->just_played()) {
    for (int v = 0; v < sys->num_voices; v++) {
      maybe_advance_to_next_section(sys, sys->render_voices[v]);
    }
  }

  ScoreCursor voice_cursors[Config::max_num_voices];
  if (transport->render_is_playing()) {
    const auto block_info = make_block_info(transport, info);
    const ScoreCursor block_size = block_info.block_size;
    const double tsig_num = block_info.tsig_num;
    const ScoreCursor global_cursor = transport->render_get_cursor_location();

    for (int v = 0; v < sys->num_voices; v++) {
      auto& voice = sys->render_voices[v];

      ScoreCursor off{};
      ScoreCursor rem = block_size;
      while (rem > ScoreCursor{}) {
        const auto& section = sys->render_sections[voice.section_index];
        const auto& clip_span = section.clip->span;
        ScoreCursor seg_beg;
        if (off == ScoreCursor{}) {
          seg_beg = global_cursor;
          seg_beg.wrapped_sub_cursor(voice.elapsed, tsig_num);
          seg_beg = clip_span.loop(seg_beg, tsig_num);
        } else {
          seg_beg = clip_span.begin;
        }

        const auto clip_end = clip_span.end(tsig_num);
        assert(clip_end > seg_beg);
        ScoreCursor dist_to_end = clip_end;
        dist_to_end.wrapped_sub_cursor(seg_beg, tsig_num);
        const ScoreCursor seg_size = std::min(dist_to_end, rem);
        const ScoreRegion seg_reg{seg_beg, seg_size};

        start_playing_notes(sys, section.clip, voice, seg_reg, off, block_info);
        stop_playing_notes(sys, section.clip, voice, seg_reg, off, block_info);

        off.wrapped_add_cursor(seg_size, tsig_num);
        rem.wrapped_sub_cursor(seg_size, tsig_num);
        if (rem > ScoreCursor{}) {
          //  crossing a clip boundary.
          voice.elapsed.wrapped_add_cursor(clip_span.size, tsig_num);
          voice.num_section_repetitions++;
          maybe_advance_to_next_section(sys, voice);
        }

        voice_cursors[v] = seg_beg;
      }
    }
  } else {
    for (int v = 0; v < sys->num_voices; v++) {
      voice_cursors[v] = sys->render_voices[v].elapsed;
    }
  }

  if (!sys->voice_feedback_to_ui.full()) {
    VoiceFeedbackToUI feedback{};
    for (int v = 0; v < sys->num_voices; v++) {
      auto& rv = sys->render_voices[v];
      VoiceFeedbackEntry entry{};
      entry.section = rv.section_index;
      entry.position = voice_cursors[v];
      entry.elapsed = rv.elapsed;
      entry.next_section = rv.next_section_index;
      entry.num_section_repetitions = rv.num_section_repetitions;
      feedback.entries[v] = entry;
    }
    sys->voice_feedback_to_ui.write(std::move(feedback));
  }
}

void ui_set_in_c(NoteClipSystem* clip_system, NoteClipHandle clip_handle, int i) {
  auto push_notes = [&](ScoreCursor* sizes, ScoreCursor* pos, PitchClass* pcs, int n, int8_t oct = 0) {
    for (int s = 0; s < n; s++) {
      ClipNote note{};
      note.span = ScoreRegion{pos[s], sizes[s]};
      note.note = MIDINote{pcs[s], int8_t(3 + oct), 127};
      ui_add_note(clip_system, clip_handle, note);
    }
  };

  if (i == 0) {
    ScoreCursor sizes[6]{{0, 1}, {0, 1}, {0, 1}, {0, 0.25}, {0, 0.25}, {0, 0.25}};
    ScoreCursor pos[6]{{0, 0}, {0, 1}, {0, 2}, {0, 0.75}, {0, 1.75}, {0, 2.75}};
    PitchClass pcs[6]{PitchClass::C, PitchClass::C, PitchClass::C, PitchClass::E, PitchClass::E, PitchClass::E};
    push_notes(sizes, pos, pcs, 6);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {0, 3}});

  } else if (i == 1) {
    ScoreCursor sizes[4]{{0, 0.5}, {0, 0.5}, {0, 1}, {0, 0.25}};
    ScoreCursor pos[4]{{0, 0}, {0, 0.5}, {0, 1}, {0, 1.75}};
    PitchClass pcs[4]{PitchClass::E, PitchClass::F, PitchClass::E, PitchClass::C};
    push_notes(sizes, pos, pcs, 4);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {0, 2}});

  } else if (i == 2) {
    ScoreCursor sizes[3]{{0, 0.5}, {0, 0.5}, {0, 0.5}};
    ScoreCursor pos[3]{{0, 0.5}, {0, 1}, {0, 1.5}};
    PitchClass pcs[3]{PitchClass::E, PitchClass::F, PitchClass::E};
    push_notes(sizes, pos, pcs, 3);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {0, 2}});

  } else if (i == 3) {
    ScoreCursor sizes[3]{{0, 0.5}, {0, 0.5}, {0, 0.5}};
    ScoreCursor pos[3]{{0, 0.5}, {0, 1}, {0, 1.5}};
    PitchClass pcs[3]{PitchClass::E, PitchClass::F, PitchClass::G};
    push_notes(sizes, pos, pcs, 3);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {0, 2}});

  } else if (i == 4) {
    ScoreCursor sizes[3]{{0, 0.5}, {0, 0.5}, {0, 0.5}};
    ScoreCursor pos[3]{{0, 0}, {0, 0.5}, {0, 1}};
    PitchClass pcs[3]{PitchClass::E, PitchClass::F, PitchClass::G};
    push_notes(sizes, pos, pcs, 3);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {0, 2}});

  } else if (i == 5) {
    ScoreCursor sizes[1]{{1, 2}};
    ScoreCursor pos[1]{{0, 0}};
    PitchClass pcs[1]{PitchClass::C};
    push_notes(sizes, pos, pcs, 1, 1);
    ui_set_clip_span(clip_system, clip_handle, ScoreRegion{{}, {1, 2}});
  }
}

bool ui_send_next_section_indices(NoteClipStateMachineSystem* sys) {
  bool all_sent{true};

  for (int v = 0; v < sys->num_voices; v++) {
    auto& voice = sys->ui_voices[v];
    if (voice.handoff_next_section_index.awaiting_read) {
      (void) acknowledged(&voice.handoff_next_section_index);
    }

    if (voice.pending_next_section_index) {
      if (!voice.handoff_next_section_index.awaiting_read) {
        int nsi = voice.pending_next_section_index.value();
        publish(&voice.handoff_next_section_index, std::move(nsi));
        voice.pending_next_section_index = NullOpt{};
      } else {
        all_sent = false;
      }
    }
  }

  return all_sent;
}

struct {
  NoteClipStateMachineSystem sys;
} globals;

} //  anon

void ncsm::render_begin_process(NoteClipStateMachineSystem* sys, const AudioRenderInfo& info) {
  begin_process(sys, info);
}

void ncsm::ui_initialize(
  NoteClipStateMachineSystem* sys, const Transport* transport, NoteClipSystem* clip_sys,
  MIDIMessageStreamSystem* midi_stream_sys) {
  //
  sys->transport = transport;
  sys->clip_system = clip_sys;
  sys->midi_stream_system = midi_stream_sys;

  for (int i = 0; i < 16; i++) {
    const int vi = sys->num_voices++;
    auto& rv = sys->render_voices[vi];
    rv = {};
  }

  for (int ri = 0; ri < 2; ri++) {
    for (int i = 0; i < 6; i++) {
      const int si = sys->num_sections++;
      auto& render_section = sys->render_sections[si];
      auto& ui_section = sys->ui_sections[si];

      const auto clip_handle = ui_create_clip(sys->clip_system, ScoreRegion{{}, {1, 0}});
      render_section.clip_handle = clip_handle;
      ui_section.clip_handle = clip_handle;
      ui_set_in_c(sys->clip_system, ui_section.clip_handle, i);
    }
  }

  sys->initialized.store(true);
}

void ncsm::ui_update(NoteClipStateMachineSystem* sys) {
  int num_feedback = sys->voice_feedback_to_ui.size();
  for (int f = 0; f < num_feedback; f++) {
    VoiceFeedbackToUI feedback = sys->voice_feedback_to_ui.read();
    for (int v = 0; v < sys->num_voices; v++) {
      auto& fb_entry = feedback.entries[v];
      auto& ui_voice = sys->ui_voices[v];
      ui_voice.section_index = fb_entry.section;
      ui_voice.approximate_next_section_index = fb_entry.next_section;
      ui_voice.approximate_cursor_position = fb_entry.position;
      ui_voice.approximate_elapsed = fb_entry.elapsed;
      ui_voice.approximate_num_section_repetitions = fb_entry.num_section_repetitions;
    }
  }

  (void) ui_send_next_section_indices(sys);
}

int ncsm::ui_get_num_sections(NoteClipStateMachineSystem* sys) {
  return sys->num_sections;
}

int ncsm::ui_get_num_voices(NoteClipStateMachineSystem* sys) {
  return sys->num_voices;
}

NoteClipStateMachineReadSection ncsm::ui_read_section(NoteClipStateMachineSystem* sys, int si) {
  assert(si >= 0 && si < sys->num_sections);
  NoteClipStateMachineReadSection result{};
  result.clip_handle = sys->ui_sections[si].clip_handle;
  return result;
}

NoteClipStateMachineReadVoice ncsm::ui_read_voice(NoteClipStateMachineSystem* sys, int vi) {
  assert(vi >= 0 && vi < sys->num_voices);
  auto& voice = sys->ui_voices[vi];
  NoteClipStateMachineReadVoice result{};
  result.position = voice.approximate_cursor_position;
  result.section = voice.section_index;
  result.next_section = voice.approximate_next_section_index;
  result.num_section_repetitions = voice.approximate_num_section_repetitions;
  return result;
}

void ncsm::ui_maybe_insert_recorded_note(
  NoteClipStateMachineSystem* sys, NoteClipSystem* clip_sys, int vi, const ClipNote& note) {
  //
  assert(!note.span.empty());
  assert(vi >= 0 && vi < sys->num_voices);
  double tsig_num = reference_time_signature().numerator;

  auto& voice = sys->ui_voices[vi];
  auto& section = sys->ui_sections[voice.section_index];
  auto* clip = ui_read_clip(clip_sys, section.clip_handle);
  assert(clip);

  auto clip_end = clip->span.end(tsig_num);
  auto note_beg = note.span.begin;
  note_beg.wrapped_sub_cursor(voice.approximate_elapsed, tsig_num);
  note_beg = clip->span.loop(note_beg, tsig_num);
  auto note_end = note_beg;
  note_end.wrapped_add_cursor(note.span.size, tsig_num);
  note_end = std::min(note_end, clip_end);
  auto note_span = ScoreRegion::from_begin_end(note_beg, note_end, tsig_num);

  auto new_note = note;
  new_note.span = note_span;
  ui_add_note(clip_sys, section.clip_handle, new_note);
}

void ncsm::ui_set_next_section_index(NoteClipStateMachineSystem* sys, int vi, int si) {
  assert(vi >= 0 && vi < sys->num_voices);
  assert(si >= 0 && si < sys->num_sections);
  sys->ui_voices[vi].pending_next_section_index = si;
}

bool ncsm::ui_send_next_section_indices_sync(NoteClipStateMachineSystem* sys, float timeout) {
  Stopwatch stopwatch;
  while (stopwatch.delta().count() < timeout) {
    if (ui_send_next_section_indices(sys)) {
      return true;
    }
  }
  return false;
}

void ncsm::ui_return_voice(NoteClipStateMachineSystem* sys, int vi) {
  assert(vi >= 0 && vi < sys->num_voices);
  assert(sys->ui_voices[vi].acquired);
  sys->ui_voices[vi].acquired = false;
  sys->ui_voices[vi].midi_message_stream_id.store(0);
}

int ncsm::ui_acquire_next_voice(
  NoteClipStateMachineSystem* sys, const MIDIMessageStreamHandle& stream) {
  //
  for (int i = 0; i < sys->num_voices; i++) {
    if (!sys->ui_voices[i].acquired) {
      sys->ui_voices[i].acquired = true;
      sys->ui_voices[i].midi_message_stream_id.store(stream.id);
      return i;
    }
  }
  assert(false);
  return -1;
}

NoteClipStateMachineSystem* ncsm::get_global_note_clip_state_machine() {
  return &globals.sys;
}

uint8_t ncsm::get_midi_source_id() {
  return Config::midi_source_id;
}

GROVE_NAMESPACE_END
