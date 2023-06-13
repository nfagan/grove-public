#pragma once

#include "types.hpp"
#include "AudioBufferStore.hpp"
#include "Transport.hpp"
#include "audio_node.hpp"
#include "NoteClipSystem.hpp"
#include "NoteQueue.hpp"
#include "NoteNumberSet.hpp"
#include "TriggeredNotes.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Handshake.hpp"

namespace grove {

struct TimelineAudioTrackHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TimelineAudioTrackHandle, id)
  uint32_t id;
};

struct TimelineAudioClipHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TimelineAudioClipHandle, id)
  uint32_t id;
};

struct TimelineNoteClipTrackHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TimelineNoteClipTrackHandle, id)
  uint32_t id;
};

struct TimelineAudioClip {
  TimelineAudioClipHandle handle{};
  AudioBufferHandle buffer;
  ScoreRegion span{};
  ScoreCursor buffer_start_offset{};
};

struct TimelineAudioTrack {
  ScoreCursor latest_span_end() const;

  TimelineAudioTrackHandle handle;
  std::vector<TimelineAudioClip> clips;
  Optional<ScoreRegion> loop_region;
  ScoreCursor start_offset{};
  ScoreCursor ui_approximate_cursor_position{};
};

struct TimelineAudioTrackRenderContext {
  const AudioBufferStore* buffer_store;
  const Transport* transport;
  const AudioRenderInfo* render_info;
};

struct TimelineTrackRenderFeedback {
  ScoreCursor cursor_position;
};

struct PlayingClipNote {
  ClipNote note;
  NoteClipHandle src_clip_handle;
  const NoteClip* src_clip;
  bool marked;
  uint64_t frame_on;
};

struct TimelineNoteClipTrackRenderData {
  DynamicArray<PlayingClipNote, 256> playing_notes;
  DynamicArray<MIDIStreamMessage, 256> pending_messages;
};

struct TimelineNoteClipTrack {
  TimelineNoteClipTrackHandle handle;
  uint32_t midi_stream_id{};
  uint8_t midi_channel{};
  std::vector<NoteClipHandle> clips;
  Optional<ScoreRegion> loop_region;
  std::shared_ptr<TimelineNoteClipTrackRenderData> render_data;
  ScoreCursor ui_approximate_cursor_position;
};

struct TimelineNoteClipTracks {
  using Tracks = std::vector<TimelineNoteClipTrack>;
  std::unique_ptr<Tracks> tracks0;
  std::unique_ptr<Tracks> tracks1;
  std::unique_ptr<Tracks> tracks2;
  bool modified;
};

struct TimelineAudioTracks {
  using Tracks = std::vector<TimelineAudioTrack>;
  std::unique_ptr<Tracks> tracks0;
  std::unique_ptr<Tracks> tracks1;
  std::unique_ptr<Tracks> tracks2;
  bool modified;
};

struct TimelineSystem {
public:
  struct RenderData {
    const std::vector<TimelineAudioTrack>* audio_tracks;
    const std::vector<TimelineNoteClipTrack>* note_clip_tracks;
  };

public:
  NoteClipSystem* clip_system{};
  MIDIMessageStreamSystem* midi_message_stream_system{};
  const AudioBufferStore* buffer_store{};
  const Transport* transport{};

  TimelineAudioTracks audio_tracks;
  TimelineNoteClipTracks note_clip_tracks;

  Handshake<RenderData> handoff_data;
  RenderData render_data{};
  RingBuffer<TimelineTrackRenderFeedback, 32> render_feedback;

  uint32_t next_track_id{1};
  uint32_t next_clip_id{1};
};

class TimelineAudioTrackNode : public AudioProcessorNode {
public:
  TimelineAudioTrackNode(TimelineSystem* timeline_system, TimelineAudioTrackHandle handle,
                         int num_output_channels);
  ~TimelineAudioTrackNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  TimelineSystem* timeline_system;

  TimelineAudioTrackHandle track_handle;
  OutputAudioPorts output_ports;
};

class TimelineNoteClipTrackNode : public AudioProcessorNode {
public:
  TimelineNoteClipTrackNode(const TimelineSystem* system, TimelineNoteClipTrackHandle handle);
  ~TimelineNoteClipTrackNode() override = default;
  GROVE_DECLARE_AUDIO_NODE_INTERFACE()

private:
  const TimelineSystem* system;
  TimelineNoteClipTrackHandle track_handle;
  OutputAudioPorts output_ports;
};

void ui_initialize(TimelineSystem* system, NoteClipSystem* clip_system,
                   MIDIMessageStreamSystem* midi_message_stream_system,
                   const Transport* transport, const AudioBufferStore* buffer_store);
void ui_update(TimelineSystem* system);

TimelineAudioTrackHandle ui_create_audio_track(TimelineSystem* system);
void ui_destroy_audio_track(TimelineSystem* system, TimelineAudioTrackHandle handle);
const TimelineAudioTrack* ui_read_audio_track(const TimelineSystem* system,
                                              TimelineAudioTrackHandle handle);

TimelineAudioClipHandle ui_create_timeline_audio_clip(TimelineSystem* system,
                                                      TimelineAudioTrackHandle track,
                                                      AudioBufferHandle buffer,
                                                      ScoreRegion clip_span);
void ui_destroy_timeline_audio_clip(TimelineSystem* sys, TimelineAudioTrackHandle track,
                                    TimelineAudioClipHandle clip);
void ui_set_timeline_audio_clip_span(TimelineSystem* sys, TimelineAudioTrackHandle track,
                                     TimelineAudioClipHandle clip, ScoreRegion span);

void ui_set_track_loop_region(TimelineSystem* sys, TimelineAudioTrackHandle track,
                              ScoreRegion region);

TimelineNoteClipTrackHandle ui_create_note_clip_track(TimelineSystem* system, uint32_t midi_stream_id);
void ui_destroy_note_clip_track(TimelineSystem* sys, TimelineNoteClipTrackHandle handle);
bool ui_is_note_clip_track(TimelineSystem* sys, TimelineNoteClipTrackHandle handle);
NoteClipHandle ui_create_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                                            ScoreRegion clip_span);
NoteClipHandle ui_duplicate_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                                               NoteClipHandle src);
NoteClipHandle ui_paste_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle dst_track,
                                           NoteClipHandle src_clip, ScoreRegion dst_clip_span);
NoteClipHandle ui_paste_timeline_note_clip_at_end(TimelineSystem* sys, TimelineNoteClipTrackHandle dst_track,
                                                  NoteClipHandle src_clip);
void ui_set_timeline_note_clip_span(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                                    NoteClipHandle clip, ScoreRegion span);
void ui_destroy_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                                   NoteClipHandle handle);
void ui_set_track_loop_region(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                              ScoreRegion region);
const TimelineNoteClipTrack* ui_read_note_clip_track(const TimelineSystem* system,
                                                     TimelineNoteClipTrackHandle handle);
ScoreCursor ui_get_track_span_end(const TimelineSystem* sys, TimelineNoteClipTrackHandle track);

bool ui_maybe_insert_recorded_note(TimelineSystem* sys, TimelineNoteClipTrackHandle track,
                                   ClipNote note);

void process(
  TimelineSystem* system, const TriggeredNotes* triggered_notes, const AudioRenderInfo& info);

}