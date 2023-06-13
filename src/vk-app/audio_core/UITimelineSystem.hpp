#pragma once

#include "grove/audio/TimelineSystem.hpp"
#include "grove/audio/ArpeggiatorSystem.hpp"
#include "AudioNodeStorage.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/SlotLists.hpp"

namespace grove {

class AudioNodeStorage;
class AudioConnectionManager;

class UITimelineSystem {
public:
  using ProcessorNodeIt = SlotLists<AudioNodeStorage::NodeID>::ConstSequenceIterator;

  struct PendingPlayResult {
    TimelineNoteClipTrackHandle track_handle;
    std::unique_ptr<TriggeredNotes::PlayResult> result;
  };

  struct NoteClipTrackNode {
    TimelineNoteClipTrackHandle track_handle;
    uint32_t midi_stream_id;
    SlotLists<AudioNodeStorage::NodeID>::List processor_nodes;
    ArpeggiatorInstanceHandle arp;
    bool is_recording;
    bool midi_output_enabled;
    bool arp_output_enabled;
  };

  struct AudioTrackNode {
    TimelineAudioTrackHandle track_handle;
    AudioNodeStorage::NodeID processor_node;
  };

public:
  void initialize(TimelineSystem& sys, NoteClipSystem& note_clip_system,
                  MIDIMessageStreamSystem& midi_message_stream_system,
                  const Transport* transport,
                  const AudioBufferStore* buffer_store);
  void end_update(TimelineSystem& sys);

  TimelineAudioTrackHandle create_audio_track(TimelineSystem& timeline_system,
                                              AudioNodeStorage& node_storage);
  void destroy_audio_track(TimelineAudioTrackHandle handle,
                           TimelineSystem& sys,
                           AudioConnectionManager& connection_manager);

  TimelineNoteClipTrackHandle create_note_clip_track(
    TimelineSystem& timeline_system, ArpeggiatorSystem& arp_system, AudioNodeStorage& node_storage,
    const PitchSampleSetGroupHandle& pitch_sample_group);

  void destroy_note_clip_track(
    TimelineNoteClipTrackHandle handle, TimelineSystem& timeline_system, TriggeredNotes& notes,
    ArpeggiatorSystem& arp_system, AudioConnectionManager& connection_manager);

  void set_recording_enabled(TimelineNoteClipTrackHandle handle, bool enabled);
  void toggle_recording_enabled(TimelineNoteClipTrackHandle handle);
  void set_midi_output_enabled(MIDIMessageStreamSystem& midi_stream_sys, TriggeredNotes& notes,
                               TimelineNoteClipTrackHandle handle, bool enabled);
  void toggle_midi_output_enabled(
    MIDIMessageStreamSystem& midi_stream_sys, TriggeredNotes& notes, TimelineNoteClipTrackHandle handle);
  void toggle_arp_enabled(
    TimelineSystem& timeline_sys,
    ArpeggiatorSystem& arp_sys, TimelineNoteClipTrackHandle handle);

  void note_on(TriggeredNotes& notes, MIDINote note);
  void note_on_timeout(TriggeredNotes& notes, MIDINote note, float s);
  void note_off(TriggeredNotes& notes, MIDINote note);

  ArrayView<const AudioTrackNode> read_audio_track_nodes() const {
    return make_view(audio_tracks);
  }
  ArrayView<const NoteClipTrackNode> read_note_clip_track_nodes() const {
    return make_view(note_clip_tracks);
  }
  const NoteClipTrackNode* read_note_clip_track_node(TimelineNoteClipTrackHandle track) const;

  ProcessorNodeIt read_processor_nodes(const NoteClipTrackNode& node) const {
    return note_clip_track_nodes.cbegin(node.processor_nodes);
  }
  int num_processor_nodes(const NoteClipTrackNode& node) const {
    return int(note_clip_track_nodes.size(node.processor_nodes));
  }
  ProcessorNodeIt end_processor_nodes() const {
    return note_clip_track_nodes.cend();
  }

public:
  std::vector<AudioTrackNode> audio_tracks;
  std::vector<NoteClipTrackNode> note_clip_tracks;
  SlotLists<AudioNodeStorage::NodeID> note_clip_track_nodes;

  std::vector<std::unique_ptr<TriggeredNotes::PlayResult>> free_note_play_results;
  std::vector<PendingPlayResult> pending_note_play_results;
};

}