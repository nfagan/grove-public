#include "TimelineSystem.hpp"
#include "MIDIMessageStreamSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/logging.hpp"
#include <unordered_set>

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr uint8_t note_clip_midi_message_source_id = 1;
};

struct TimelineNoteClipTrackRenderContext {
  const AudioRenderInfo* render_info;
  const NoteClipSystem* clip_system;
  const Transport* transport;
  const TriggeredNotes* triggered_notes;
  MIDIMessageStreamSystem* midi_message_stream_system;
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "TimelineSystem";
}

[[maybe_unused]] bool is_valid_loop_region(const ScoreRegion& region) {
  return region.begin >= ScoreCursor{} && region.size > ScoreCursor{};
}

[[maybe_unused]] bool is_valid_clip_span(const ScoreRegion& region) {
  return region.begin >= ScoreCursor{} && region.size > ScoreCursor{};
}

[[maybe_unused]] bool playing_notes_are_valid(const PlayingClipNote* beg, const PlayingClipNote* end) {
  std::unordered_set<uint8_t> note_numbers;
  for (; beg != end; ++beg) {
    auto note_num = beg->note.note.note_number();
    if (note_numbers.count(note_num)) {
      return false;
    } else {
      note_numbers.insert(note_num);
    }
  }
  return true;
}

bool is_playing_note(const PlayingClipNote* beg, const PlayingClipNote* end, MIDINote note) {
  auto curr_it = std::find_if(beg, end, [note](const PlayingClipNote& current_note) {
    return current_note.note.note.matches_pitch_class_and_octave(note);
  });
  return curr_it != end;
}

[[maybe_unused]] bool render_is_playing_note(const TimelineNoteClipTrack& track, MIDINote note) {
  return is_playing_note(
    track.render_data->playing_notes.begin(),
    track.render_data->playing_notes.end(),
    note);
}

[[maybe_unused]] void validate_playing_notes(const PlayingClipNote* beg,
                                             const PlayingClipNote* end) {
  assert(playing_notes_are_valid(beg, end));
  (void) beg;
  (void) end;
}

ScoreCursor to_local_cursor_position(const TimelineAudioTrack& track,
                                     ScoreCursor transport_cursor, double num) {
  transport_cursor.wrapped_add_cursor(track.start_offset, num);
  if (track.loop_region) {
    return track.loop_region.value().loop(transport_cursor, num);
  } else {
    return transport_cursor;
  }
}

const Transport* render_get_transport(const TimelineSystem* sys) {
  assert(sys->transport);
  return sys->transport;
}

const AudioBufferStore* render_get_buffer_store(const TimelineSystem* sys) {
  assert(sys->buffer_store);
  return sys->buffer_store;
}

const TimelineAudioTrack* render_get_timeline_audio_track(const TimelineSystem* system,
                                                          TimelineAudioTrackHandle handle) {
  if (system->render_data.audio_tracks) {
    for (auto& track : *system->render_data.audio_tracks) {
      if (track.handle == handle) {
        return &track;
      }
    }
  }
  return nullptr;
}

const TimelineNoteClipTrack*
render_get_timeline_note_clip_track(const TimelineSystem* sys, TimelineNoteClipTrackHandle handle) {
  if (sys->render_data.note_clip_tracks) {
    for (auto& track : *sys->render_data.note_clip_tracks) {
      if (track.handle == handle) {
        return &track;
      }
    }
  }
  return nullptr;
}

TimelineAudioTrack* ui_find_track(TimelineSystem* system, TimelineAudioTrackHandle handle) {
  for (auto& track : *system->audio_tracks.tracks0) {
    if (track.handle == handle) {
      return &track;
    }
  }
  return nullptr;
}

const TimelineAudioTrack* ui_find_track(const TimelineSystem* system,
                                        TimelineAudioTrackHandle handle) {
  for (auto& track : *system->audio_tracks.tracks0) {
    if (track.handle == handle) {
      return &track;
    }
  }
  return nullptr;
}

auto find_audio_clip(TimelineAudioTrack* track, TimelineAudioClipHandle clip_handle) {
  return std::find_if(track->clips.begin(), track->clips.end(), [clip_handle](const auto& clip) {
    return clip.handle == clip_handle;
  });
}

TimelineNoteClipTrack* ui_find_track(TimelineSystem* system, TimelineNoteClipTrackHandle handle) {
  for (auto& track : *system->note_clip_tracks.tracks0) {
    if (track.handle == handle) {
      return &track;
    }
  }
  return nullptr;
}

const TimelineNoteClipTrack* ui_find_track(const TimelineSystem* system,
                                           TimelineNoteClipTrackHandle handle) {
  for (auto& track : *system->note_clip_tracks.tracks0) {
    if (track.handle == handle) {
      return &track;
    }
  }
  return nullptr;
}

auto partition_intersecting_regions(ScoreRegion to_partition, ScoreRegion intersecting_region,
                                    double beats_per_measure) {
  struct Result {
    Optional<ScoreRegion> pre_part;
    Optional<ScoreRegion> post_part;
  };

  Result result{};

  if (to_partition.begin < intersecting_region.begin) {
    auto sz = intersecting_region.begin;
    sz.wrapped_sub_cursor(to_partition.begin, beats_per_measure);

    ScoreRegion pre_part{};
    pre_part.begin = to_partition.begin;
    pre_part.size = sz;
    result.pre_part = pre_part;
  }

  const auto part_end = to_partition.end(beats_per_measure);
  const auto isect_end = intersecting_region.end(beats_per_measure);
  if (part_end > isect_end) {
    auto post_sz = part_end;
    post_sz.wrapped_sub_cursor(isect_end, beats_per_measure);

    ScoreRegion post_part{};
    post_part.begin = isect_end;
    post_part.size = post_sz;
    result.post_part = post_part;
  }

  return result;
}

bool render_maybe_push_latest_cursor_position(TimelineSystem* system, ScoreCursor cursor) {
  TimelineTrackRenderFeedback feedback{};
  feedback.cursor_position = cursor;
  return system->render_feedback.maybe_write(feedback);
}

struct TransportPlaybackInfo {
  ScoreCursor cursor;
  TimeSignature tsig;
  double beats_per_sample;
  ScoreCursor sample_size;
};

TransportPlaybackInfo get_transport_playback_info(const Transport* transport, double sample_rate) {
  TransportPlaybackInfo result{};
  auto tsig = reference_time_signature();
  auto bps = beats_per_sample_at_bpm(transport->get_bpm(), sample_rate, tsig);
  result.cursor = transport->render_get_cursor_location();
  result.tsig = tsig;
  result.beats_per_sample = bps;
  result.sample_size = ScoreCursor::from_beats(bps, tsig.numerator);
  return result;
}

void render_timeline_audio_track(const TimelineAudioTrack& track, const AudioProcessData& dst,
                                 const TransportPlaybackInfo& playback_info,
                                 const TimelineAudioTrackRenderContext& context) {

#ifdef GROVE_DEBUG
  for (auto& channel : dst.descriptors) {
    assert(channel.is_float());
  }
#endif
  const int num_channels = int(dst.descriptors.size());

  auto transport_cursor = playback_info.cursor;
  auto transport_tsig = playback_info.tsig;
  auto bps = playback_info.beats_per_sample;
  const double spb = 1.0 / bps;

  auto cursor = transport_cursor;
  auto sample_size = ScoreCursor::from_beats(bps, transport_tsig.numerator);

  if (!context.transport->render_is_playing()) {
    return;
  }

  for (int i = 0; i < context.render_info->num_frames; i++) {
    const auto local_pos = to_local_cursor_position(track, cursor, transport_tsig.numerator);
    cursor.wrapped_add_cursor(sample_size, transport_tsig.numerator);

    //  @TODO: Don't do this lookup every frame.
    auto clip_it = std::find_if(
      track.clips.begin(), track.clips.end(), [local_pos, transport_tsig](const auto& clip) {
        return clip.span.contains(local_pos, transport_tsig.numerator);
      });

    if (clip_it == track.clips.end()) {
      continue;
    }

    const auto& clip = *clip_it;
    auto buffer_pos = local_pos;
    buffer_pos.wrapped_sub_cursor(clip.span.begin, transport_tsig.numerator);
    buffer_pos.wrapped_add_cursor(clip.buffer_start_offset, transport_tsig.numerator);

    double sample_index = buffer_pos.to_sample_offset(spb, transport_tsig.numerator);
    if (sample_index < 0.0) {
      //  buffer_start_offset offset can be negative.
      continue;
    }

    //  @TODO: Don't do this lookup every frame.
    auto maybe_buff = context.buffer_store->render_get(
      clip.buffer, sample_index, *context.render_info);
    if (!maybe_buff || !maybe_buff.value().descriptor.is_n_channel_float(num_channels)) {
      continue;
    }

    auto& buff = maybe_buff.value();

    if (uint64_t(sample_index) >= buff.num_frames_in_source()) {
      //  clip region can be longer than the audio sample.
      continue;
    }

    auto interp = util::make_linear_interpolation_info(sample_index, buff.num_frames_in_source());
    for (int j = 0; j < num_channels; j++) {
      const float v = util::tick_interpolated_float(buff.data, buff.channel_descriptor(j), interp);
      dst.descriptors[j].write(dst.buffer.data, i, &v);
    }
  }
}

PlayingClipNote make_playing_clip_note(ClipNote note, NoteClipHandle clip_handle,
                                       const NoteClip* clip, uint64_t frame_on) {
  PlayingClipNote playing{};
  playing.note = note;
  playing.src_clip_handle = clip_handle;
  playing.src_clip = clip;
  playing.frame_on = frame_on;
  return playing;
}

const NoteClip* find_clip_containing_cursor(const NoteClipSystem* sys, ScoreCursor cursor,
                                            double beats_per_measure,
                                            const NoteClipHandle* handles, int num_handles,
                                            int* clip_handle_index) {
  for (int i = 0; i < num_handles; i++) {
    if (auto* clip = render_read_clip(sys, handles[i])) {
      if (clip->span.contains(cursor, beats_per_measure)) {
        *clip_handle_index = i;
        return clip;
      }
    }
  }
  return nullptr;
}

void note_off(const TimelineNoteClipTrack& track, const ClipNote& note, int frame, uint8_t source_id) {
  MIDIStreamMessage msg{};
  msg.message = MIDIMessage::make_note_off(track.midi_channel, note.note);
  msg.source_id = source_id;
  msg.frame = frame;
  track.render_data->pending_messages.push_back(msg);
}

void note_off_clear_playing(const TimelineNoteClipTrack& track, int frame, uint8_t source_id) {
  for (auto& note : track.render_data->playing_notes) {
    note_off(track, note.note, frame, source_id);
  }
  track.render_data->playing_notes.clear();
}

void note_on(const TimelineNoteClipTrack& track, const ClipNote& note, int frame, uint8_t source_id) {
  MIDIStreamMessage msg{};
  msg.message = MIDIMessage::make_note_on(track.midi_channel, note.note);
  msg.source_id = source_id;
  msg.frame = frame;
  track.render_data->pending_messages.push_back(msg);
}

bool remove_note(const PlayingClipNote& note, double beats_per_measure,
                 const TimelineNoteClipTrackRenderContext& context, const NoteClip** dst_clip) {
  auto* clip = render_read_clip(context.clip_system, note.src_clip_handle);
  if (!clip) {
    //  The clip containing this note has been deleted.
    return true;
  }

  auto& clip_note = note.note;
  auto* src_note = render_find_note(
    context.clip_system, clip,
    clip_note.span.begin,
    clip_note.span.end(beats_per_measure),
    clip_note.note);

  if (!src_note || *src_note != note.note) {
    //  The note has been moved or deleted.
    //  @TODO: We can keep playing notes that are shifted in time or resized as long as the cursor
    //  remains within the note.
    return true;
  }

  *dst_clip = clip;
  return false;
}

void remove_expired(const TimelineNoteClipTrack& track, const TransportPlaybackInfo& playback_info,
                    const TimelineNoteClipTrackRenderContext& context) {
  auto& playing_notes = track.render_data->playing_notes;
  int num_playing = int(playing_notes.size());
  const double beats_per_measure = playback_info.tsig.numerator;

  int ni{};
  for (int i = 0; i < num_playing; i++) {
    auto& note = playing_notes[ni];
    const NoteClip* src_clip{};
    if (remove_note(note, beats_per_measure, context, &src_clip)) {
      note_off(track, note.note, 0, Config::note_clip_midi_message_source_id);
      playing_notes.erase(playing_notes.begin() + ni);
    } else {
      note.src_clip = src_clip;
      ++ni;
    }
  }
}

struct GatherNoteClip {
  const NoteClip* clip;
  NoteClipHandle clip_handle;
};

//  @TODO: Accelerate this lookup.
int find_clips_intersecting_interval(const NoteClipSystem* sys,
                                     const std::vector<NoteClipHandle>& clips,
                                     const ScoreRegion& span, double num,
                                     GatherNoteClip* dst, int max_num_dst) {
  int dst_ind{};
  for (auto& clip_handle : clips) {
    auto* clip = render_read_clip(sys, clip_handle);
    if (clip && clip->span.intersects(span, num)) {
      if (dst_ind < max_num_dst) {
        dst[dst_ind] = {clip, clip_handle};
      }
      dst_ind++;
    }
  }

  int num_sort = std::min(dst_ind, max_num_dst);
  std::sort(dst, dst + num_sort, [](const GatherNoteClip& a, const GatherNoteClip& b) {
    return a.clip->span.begin < b.clip->span.begin;
  });
  return dst_ind;
}

bool ignore_note(const TimelineNoteClipTrack& track, const TriggeredNotes* notes, MIDINote note) {
  return notes::render_is_playing_note(notes, track.midi_stream_id, note);
}

void start_playing_notes(const TimelineNoteClipTrack& track,
                         const TransportPlaybackInfo& playback_info,
                         const TimelineNoteClipTrackRenderContext& context) {
  const int num_frames = context.render_info->num_frames;
  if (num_frames == 0) {
    return;
  }

  const double num = playback_info.tsig.numerator;
  const auto cursor_begin = playback_info.cursor;
  const auto block_size = ScoreCursor::from_beats(playback_info.beats_per_sample * num_frames, num);
  const double samples_per_beat = 1.0 / playback_info.beats_per_sample;
  const uint64_t start_frame = context.render_info->render_frame;

  constexpr int interval_stack_size = 32;
  Temporary<ScoreRegionSegment, interval_stack_size> interval_store;
  ScoreRegionSegment* intervals = interval_store.require(interval_stack_size);
  int num_intervals{};

  if (track.loop_region) {
    auto beg = track.loop_region.value().loop(cursor_begin, num);
    num_intervals = partition_loop(
      ScoreRegion{beg, block_size},
      track.loop_region.value(),
      num, intervals, interval_stack_size);
    assert(num_intervals <= interval_stack_size);
    num_intervals = std::min(num_intervals, interval_stack_size);
  } else {
    intervals[0] = ScoreRegionSegment{ScoreRegion{cursor_begin, block_size}, {}};
    num_intervals = 1;
  }

  for (int i = 0; i < num_intervals; i++) {
    const auto& interval = intervals[i];

    constexpr int clip_stack_size = 256;
    Temporary<GatherNoteClip, clip_stack_size> clip_store;
    auto* clips = clip_store.require(clip_stack_size);

    int num_clips = find_clips_intersecting_interval(
      context.clip_system, track.clips, interval.span, num, clips, clip_stack_size);
    assert(num_clips <= clip_stack_size);
    num_clips = std::min(num_clips, clip_stack_size);

    constexpr int note_stack_size = 1024;
    Temporary<uint32_t, note_stack_size> note_indices_store;
    Temporary<ClipNote, note_stack_size> note_store;
    uint32_t* note_indices = note_indices_store.require(note_stack_size);
    ClipNote* notes = note_store.require(note_stack_size);

    for (int j = 0; j < num_clips; j++) {
      const NoteClip* clip = clips[j].clip;
      const NoteClipHandle clip_handle = clips[j].clip_handle;

      auto& clip_span = clip->span;
      auto isect_span = intersect_of(clip_span, interval.span, num);
      assert(isect_span.size > ScoreCursor{});

      auto search_span = isect_span;
      search_span.begin.wrapped_sub_cursor(clip_span.begin, num);

      int num_notes = render_collect_notes_starting_in_region(
        context.clip_system, clip, search_span, note_indices, notes, note_stack_size);
      assert(num_notes <= note_stack_size);
      num_notes = std::min(num_notes, note_stack_size);

      for (int k = 0; k < num_notes; k++) {
        auto& note = notes[k];
        if (ignore_note(track, context.triggered_notes, note.note)) {
          continue;
        }

        auto block_rel_begin = note.span.begin;
        block_rel_begin.wrapped_add_cursor(clip_span.begin, num);
        block_rel_begin.wrapped_sub_cursor(interval.span.begin, num);
        block_rel_begin.wrapped_add_cursor(interval.cumulative_offset, num);
        auto beat_rel = block_rel_begin.to_beats(num);
        auto sample_beg = samples_per_beat * beat_rel;
        int frame = int(std::floor(sample_beg));
        assert(frame >= 0 && frame < num_frames);
        frame = std::max(0, std::min(frame, num_frames - 1));
#if 0
        double leftover_beats = (sample_beg - frame) * beats_per_sample;
        ScoreCursor leftover_cursor = ScoreCursor::from_beats(leftover_beats, num);
        assert(leftover_cursor >= ScoreCursor{});
        auto frame_cursor = ScoreCursor::from_beats(frame * beats_per_sample, num);
        leftover_cursor.wrapped_sub_cursor(frame_cursor, num);
#endif
        const uint64_t frame_on = frame + start_frame;
        auto playing_note = make_playing_clip_note(note, clip_handle, clip, frame_on);
        track.render_data->playing_notes.push_back(playing_note);
        note_on(track, note, frame, Config::note_clip_midi_message_source_id);
      }
    }
  }
}

void update_playing_notes_interval(const TimelineNoteClipTrack& track,
                                   const TransportPlaybackInfo& playback_info,
                                   const TimelineNoteClipTrackRenderContext& context,
                                   const ScoreRegionSegment& interval) {
  auto& playing_notes = track.render_data->playing_notes;
  const double num = playback_info.tsig.numerator;
  const double samples_per_beat = 1.0 / playback_info.beats_per_sample;
  const int num_frames = context.render_info->num_frames;
  const uint64_t start_frame = context.render_info->render_frame;
  const ScoreCursor loop_end = track.loop_region ?
    track.loop_region.value().end(num) :
    ScoreCursor{std::numeric_limits<int64_t>::max(), 0.0};

  int ni{};
  const auto num_playing = int(playing_notes.size());
  for (int i = 0; i < num_playing; i++) {
    auto& note = playing_notes[ni];
    auto note_beg = note.note.span.begin;
    note_beg.wrapped_add_cursor(note.src_clip->span.begin, num);
    auto note_end = note.note.span.end(num);
    note_end.wrapped_add_cursor(note.src_clip->span.begin, num);
//    auto true_note_end = note_end;
    note_end = std::min(std::min(note_end, note.src_clip->span.end(num)), loop_end);

    ScoreRegion note_span = ScoreRegion::from_begin_end(note_beg, note_end, num);
    if (!interval.span.intersects(note_span, num)) {
      note.marked = false;
      ++ni;
      continue;
    }

    note.marked = true;
    bool remove{};

    if (note_end > interval.span.begin && note_end <= interval.span.end(num)) {
      auto block_rel = note_end;
      block_rel.wrapped_sub_cursor(interval.span.begin, num);
      block_rel.wrapped_add_cursor(interval.cumulative_offset, num);

      auto beat_rel = block_rel.to_beats(num);
      auto sample_beg = samples_per_beat * beat_rel;
      int frame = int(std::ceil(sample_beg));
      assert(frame > 0 && frame <= num_frames + 1);
      frame = std::min(std::max(0, frame - 1), num_frames - 1);
      const uint64_t stop_frame = frame + start_frame;

      if (stop_frame > note.frame_on) {
        note_off(track, note.note, frame, Config::note_clip_midi_message_source_id);
        remove = true;
      }
    }

    if (remove) {
      playing_notes.erase(playing_notes.begin() + ni);
    } else {
      ++ni;
    }
  }
}

void update_playing_notes(const TimelineNoteClipTrack& track,
                          const TransportPlaybackInfo& playback_info,
                          const TimelineNoteClipTrackRenderContext& context) {
  const int num_frames = context.render_info->num_frames;
  if (num_frames == 0) {
    return;
  }

  const double num = playback_info.tsig.numerator;
  const auto cursor_begin = playback_info.cursor;
  const auto block_size = ScoreCursor::from_beats(playback_info.beats_per_sample * num_frames, num);

  constexpr int interval_stack_size = 32;
  Temporary<ScoreRegionSegment, interval_stack_size> interval_store;
  ScoreRegionSegment* intervals = interval_store.require(interval_stack_size);
  int num_intervals{};

  if (track.loop_region) {
    auto beg = track.loop_region.value().loop(cursor_begin, num);
    num_intervals = partition_loop(
      ScoreRegion{beg, block_size},
      track.loop_region.value(),
      num, intervals, interval_stack_size);
    assert(num_intervals <= interval_stack_size);
    num_intervals = std::min(num_intervals, interval_stack_size);
  } else {
    intervals[0] = ScoreRegionSegment{ScoreRegion{cursor_begin, block_size}, {}};
    num_intervals = 1;
  }

  auto& playing_notes = track.render_data->playing_notes;
  for (auto& note : playing_notes) {
    note.marked = false;
  }

  for (int i = 0; i < num_intervals; i++) {
    update_playing_notes_interval(track, playback_info, context, intervals[i]);
  }

  const int num_playing = int(playing_notes.size());
  int ni{};
  for (int i = 0; i < num_playing; i++) {
    if (!playing_notes[ni].marked) {
      note_off(track, playing_notes[ni].note, 0, Config::note_clip_midi_message_source_id);
      playing_notes.erase(playing_notes.begin() + ni);
    } else {
      ++ni;
    }
  }
}

void render_timeline_note_clip_track(const TimelineNoteClipTrack& track,
                                     const TransportPlaybackInfo& playback,
                                     const TimelineNoteClipTrackRenderContext& context) {
  track.render_data->pending_messages.clear();

  remove_expired(track, playback, context);

  if (context.transport->just_stopped()) {
    note_off_clear_playing(track, 0, Config::note_clip_midi_message_source_id);
  }

  if (context.transport->render_is_playing()) {
    start_playing_notes(track, playback, context);
    update_playing_notes(track, playback, context);
  }

  auto& msgs = track.render_data->pending_messages;
  midi::render_push_messages(
    context.midi_message_stream_system,
    MIDIMessageStreamHandle{track.midi_stream_id},
    msgs.data(), int(msgs.size()));

#ifdef GROVE_DEBUG
  const auto& notes = track.render_data->playing_notes;
  validate_playing_notes(notes.begin(), notes.end());
#endif
};

TimelineAudioTracks make_timeline_audio_tracks() {
  //  tracks0: ui (main) thread read / write.
  //  tracks2: must be assumed to be in use by audio render thread
  TimelineAudioTracks result;
  result.tracks0 = std::make_unique<TimelineAudioTracks::Tracks>();
  result.tracks1 = std::make_unique<TimelineAudioTracks::Tracks>();
  result.tracks2 = std::make_unique<TimelineAudioTracks::Tracks>();
  return result;
}

TimelineNoteClipTracks make_timeline_note_clip_tracks() {
  //  tracks0: ui (main) thread read / write.
  //  tracks2: must be assumed to be in use by audio render thread
  TimelineNoteClipTracks result;
  result.tracks0 = std::make_unique<TimelineNoteClipTracks::Tracks>();
  result.tracks1 = std::make_unique<TimelineNoteClipTracks::Tracks>();
  result.tracks2 = std::make_unique<TimelineNoteClipTracks::Tracks>();
  return result;
}

template <typename T>
void swap_tracks12(T& tracks) {
  std::swap(tracks.tracks1, tracks.tracks2);
}

template <typename T>
void copy_tracks1_clear_modified(T& tracks) {
  *tracks.tracks1 = *tracks.tracks0;
  tracks.modified = false;
}

TimelineNoteClipTrack make_timeline_note_clip_track(
  TimelineNoteClipTrackHandle handle, uint32_t midi_stream_id) {
  //
  TimelineNoteClipTrack result{};
  result.handle = handle;
  result.render_data = std::make_shared<TimelineNoteClipTrackRenderData>();
  result.midi_stream_id = midi_stream_id;
  return result;
}

void sort_clip_spans(std::vector<NoteClipHandle>& clips, NoteClipSystem* sys) {
  std::sort(clips.begin(), clips.end(), [sys](const NoteClipHandle& a, const NoteClipHandle& b) {
    return ui_read_clip(sys, a)->span.begin < ui_read_clip(sys, b)->span.begin;
  });
}

void sort_clip_spans(std::vector<TimelineAudioClip>& clips) {
  using Clip = TimelineAudioClip;
  std::sort(clips.begin(), clips.end(), [](const Clip& a, const Clip& b) {
    return a.span.begin < b.span.begin;
  });
}

[[maybe_unused]] bool is_sorted(const std::vector<ScoreRegion>& spans) {
  return std::is_sorted(spans.begin(), spans.end(), [](const ScoreRegion& a, const ScoreRegion& b) {
    return a.begin < b.begin;
  });
}
[[maybe_unused]] void validate_clip_spans(const std::vector<ScoreRegion>& spans) {
  for (int i = 0; i < int(spans.size()); i++) {
    for (int j = 0; j < int(spans.size()); j++) {
      if (i != j) {
        assert(!spans[i].intersects(spans[j], reference_time_signature().numerator));
      }
    }
  }
  assert(is_sorted(spans));
}

[[maybe_unused]] void validate_clip_spans(const std::vector<NoteClipHandle>& clips,
                                          NoteClipSystem* sys) {
  std::vector<ScoreRegion> spans;
  for (auto& clip_handle : clips) {
    spans.push_back(ui_read_clip(sys, clip_handle)->span);
  }
  validate_clip_spans(spans);
}

[[maybe_unused]] void validate_clip_spans(const std::vector<TimelineAudioClip>& clips) {
  std::vector<ScoreRegion> spans;
  for (auto& clip : clips) {
    spans.push_back(clip.span);
  }
  validate_clip_spans(spans);
}

void add_clip(TimelineNoteClipTrack* track, NoteClipSystem* clip_system, NoteClipHandle clip) {
  track->clips.push_back(clip);
  sort_clip_spans(track->clips, clip_system);
}

void reconcile_new_clip_span(TimelineNoteClipTrack* track, NoteClipSystem* clip_system,
                             ScoreRegion clip_span, const Optional<NoteClipHandle>& skip) {
  const auto ref_tsig = reference_time_signature();

  int clip_ind{};
  const auto num_clips = int(track->clips.size());
  for (int i = 0; i < num_clips; i++) {
    auto clip_handle = track->clips[clip_ind];
    if (skip && clip_handle == skip.value()) {
      ++clip_ind;
      continue;
    }

    auto* clip = ui_read_clip(clip_system, clip_handle);
    assert(clip);

    if (clip->span.intersects(clip_span, ref_tsig.numerator)) {
      auto segments = partition_intersecting_regions(clip->span, clip_span, ref_tsig.numerator);
      if (segments.pre_part) {
        auto pre_clip = ui_clone_clip(clip_system, clip_handle);
        ui_set_clip_span(clip_system, pre_clip, segments.pre_part.value());
        track->clips.push_back(pre_clip);
      }
      if (segments.post_part) {
        auto post_clip = ui_clone_clip(clip_system, clip_handle);
        ui_set_clip_span(clip_system, post_clip, segments.post_part.value());
        track->clips.push_back(post_clip);
      }
      ui_destroy_clip(clip_system, clip_handle);
      track->clips.erase(track->clips.begin() + clip_ind);
    } else {
      ++clip_ind;
    }
  }
}

void reconcile_new_clip_span(std::vector<TimelineAudioClip>& clips, const ScoreRegion& clip_span) {
  const double num = reference_time_signature().numerator;

  auto clip_it = clips.begin();
  while (clip_it != clips.end()) {
    const TimelineAudioClip exist_clip = *clip_it;
    auto& exist_span = exist_clip.span;
    if (exist_span.intersects(clip_span, num)) {
      clip_it = clips.erase(clip_it);

      auto segments = partition_intersecting_regions(exist_span, clip_span, num);
      if (segments.pre_part) {
        auto pre_seg = exist_clip;
        pre_seg.span = segments.pre_part.value();
        clip_it = clips.insert(clip_it, pre_seg);
      }
      if (segments.post_part) {
        auto post_seg = exist_clip;
        post_seg.span = segments.post_part.value();
        clip_it = clips.insert(clip_it, post_seg);
      }
    } else {
      ++clip_it;
    }
  }
}

auto ui_find_track_and_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle track_handle,
                            NoteClipHandle clip_handle) {
  struct Result {
    TimelineNoteClipTrack* track;
    const NoteClip* clip;
  };

  auto* track = ui_find_track(sys, track_handle);
  assert(track);

#ifdef GROVE_DEBUG
  auto it = std::find(track->clips.begin(), track->clips.end(), clip_handle);
  assert(it != track->clips.end());
#endif
  auto* clip = ui_read_clip(sys->clip_system, clip_handle);
  assert(clip);

  Result result{};
  result.track = track;
  result.clip = clip;
  return result;
}

void ui_track_handoff(TimelineSystem* system) {
  if (system->handoff_data.awaiting_read && acknowledged(&system->handoff_data)) {
    swap_tracks12(system->audio_tracks);
    swap_tracks12(system->note_clip_tracks);
  }

  if (!system->handoff_data.awaiting_read) {
    if (system->audio_tracks.modified || system->note_clip_tracks.modified) {
      copy_tracks1_clear_modified(system->audio_tracks);
      copy_tracks1_clear_modified(system->note_clip_tracks);

      TimelineSystem::RenderData handoff{};
      handoff.audio_tracks = system->audio_tracks.tracks1.get();
      handoff.note_clip_tracks = system->note_clip_tracks.tracks1.get();
      publish(&system->handoff_data, std::move(handoff));
    }
  }
}

void ui_process_render_feedback(TimelineSystem* system) {
  const int num_feedback_items = system->render_feedback.size();
  const double num = reference_time_signature().numerator;

  for (int i = 0; i < num_feedback_items; i++) {
    auto item = system->render_feedback.read();

    for (auto& track : *system->audio_tracks.tracks0) {
      auto cursor = item.cursor_position;
      if (track.loop_region) {
        cursor = track.loop_region.value().loop(cursor, num);
      }
      track.ui_approximate_cursor_position = cursor;
    }

    for (auto& track : *system->note_clip_tracks.tracks0) {
      auto cursor = item.cursor_position;
      if (track.loop_region) {
        cursor = track.loop_region.value().loop(cursor, num);
      }
      track.ui_approximate_cursor_position = cursor;
    }
  }
}

} //  anon

void ui_initialize(TimelineSystem* system, NoteClipSystem* clip_system,
                   MIDIMessageStreamSystem* midi_message_stream_system,
                   const Transport* audio_transport, const AudioBufferStore* buffer_store) {
  system->clip_system = clip_system;
  system->midi_message_stream_system = midi_message_stream_system;
  system->transport = audio_transport;
  system->buffer_store = buffer_store;
  system->audio_tracks = make_timeline_audio_tracks();
  system->note_clip_tracks = make_timeline_note_clip_tracks();
  //
  system->render_data.audio_tracks = system->audio_tracks.tracks2.get();
  system->render_data.note_clip_tracks = system->note_clip_tracks.tracks2.get();
}

void ui_update(TimelineSystem* system) {
  ui_track_handoff(system);
  ui_process_render_feedback(system);
}

void process(
  TimelineSystem* system, const TriggeredNotes* triggered_notes, const AudioRenderInfo& render_info) {
  //
  if (auto res = read(&system->handoff_data)) {
    system->render_data = res.value();
  }

  const auto playback = get_transport_playback_info(system->transport, render_info.sample_rate);

  TimelineNoteClipTrackRenderContext context{};
  context.clip_system = system->clip_system;
  context.midi_message_stream_system = system->midi_message_stream_system;
  context.render_info = &render_info;
  context.transport = system->transport;
  context.triggered_notes = triggered_notes;

  for (auto& track : *system->render_data.note_clip_tracks) {
    render_timeline_note_clip_track(track, playback, context);
  }

  render_maybe_push_latest_cursor_position(system, playback.cursor);
}

TimelineAudioTrackHandle ui_create_audio_track(TimelineSystem* system) {
  TimelineAudioTrackHandle handle{system->next_track_id++};
  auto& track = system->audio_tracks.tracks0->emplace_back();
  track.handle = handle;
  system->audio_tracks.modified = true;
  return handle;
}

void ui_destroy_audio_track(TimelineSystem* system, TimelineAudioTrackHandle handle) {
  auto& tracks = *system->audio_tracks.tracks0;
  auto track_it = std::find_if(tracks.begin(), tracks.end(), [handle](const auto& track) {
    return track.handle == handle;
  });
  if (track_it != tracks.end()) {
    tracks.erase(track_it);
    system->audio_tracks.modified = true;
  } else {
    assert(false);
  }
}

const TimelineAudioTrack*
ui_read_audio_track(const TimelineSystem* system, TimelineAudioTrackHandle handle) {
  return ui_find_track(system, handle);
}

TimelineAudioClipHandle ui_create_timeline_audio_clip(TimelineSystem* system,
                                                      TimelineAudioTrackHandle track_handle,
                                                      AudioBufferHandle buffer,
                                                      ScoreRegion clip_span) {
  assert(is_valid_clip_span(clip_span));

  auto* track = ui_find_track(system, track_handle);
  assert(track);

  reconcile_new_clip_span(track->clips, clip_span);

  TimelineAudioClipHandle handle{system->next_clip_id++};
  TimelineAudioClip clip{};
  clip.handle = handle;
  clip.span = clip_span;
  clip.buffer = buffer;

  track->clips.push_back(clip);
  sort_clip_spans(track->clips);

  system->audio_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips);
#endif
  return handle;
}

void ui_destroy_timeline_audio_clip(TimelineSystem* sys, TimelineAudioTrackHandle track_handle,
                                    TimelineAudioClipHandle clip_handle) {
  auto* track = ui_find_track(sys, track_handle);
  assert(track);
  auto clip_it = find_audio_clip(track, clip_handle);
  assert(clip_it != track->clips.end());
  track->clips.erase(clip_it);
  sys->audio_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips);
#endif
}

void ui_set_timeline_audio_clip_span(TimelineSystem* sys, TimelineAudioTrackHandle track_handle,
                                     TimelineAudioClipHandle clip_handle, ScoreRegion span) {
  assert(is_valid_clip_span(span));
  auto* track = ui_find_track(sys, track_handle);
  assert(track);
  auto clip_it = find_audio_clip(track, clip_handle);
  assert(clip_it != track->clips.end());
  auto restore = *clip_it;
  track->clips.erase(clip_it);

  reconcile_new_clip_span(track->clips, span);

  restore.span = span;
  track->clips.push_back(restore);
  sort_clip_spans(track->clips);
  sys->audio_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips);
#endif
}

void ui_set_track_loop_region(TimelineSystem* sys, TimelineAudioTrackHandle handle,
                              ScoreRegion region) {
  auto* track = ui_find_track(sys, handle);
  assert(is_valid_loop_region(region));
  track->loop_region = region;
  sys->audio_tracks.modified = true;
}

TimelineNoteClipTrackHandle
ui_create_note_clip_track(TimelineSystem* system, uint32_t midi_stream_id) {
  TimelineNoteClipTrackHandle handle{system->next_track_id++};
  system->note_clip_tracks.tracks0->emplace_back() =
    make_timeline_note_clip_track(handle, midi_stream_id);
  system->note_clip_tracks.modified = true;
  return handle;
}

bool ui_is_note_clip_track(TimelineSystem* sys, TimelineNoteClipTrackHandle handle) {
  return ui_find_track(sys, handle) != nullptr;
}

void ui_destroy_note_clip_track(TimelineSystem* sys, TimelineNoteClipTrackHandle handle) {
  auto& tracks = *sys->note_clip_tracks.tracks0;
  auto track_it = std::find_if(tracks.begin(), tracks.end(), [handle](const auto& track) {
    return track.handle == handle;
  });
  if (track_it != tracks.end()) {
    for (auto& clip : track_it->clips) {
      ui_destroy_clip(sys->clip_system, clip);
    }
    tracks.erase(track_it);
    sys->note_clip_tracks.modified = true;
  } else {
    assert(false);
  }
}

NoteClipHandle ui_create_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle handle,
                                            ScoreRegion clip_span) {
  assert(is_valid_clip_span(clip_span));
  auto res = ui_create_clip(sys->clip_system, clip_span);
  auto* track = ui_find_track(sys, handle);
  reconcile_new_clip_span(track, sys->clip_system, clip_span, NullOpt{});
  add_clip(track, sys->clip_system, res);
  sys->note_clip_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips, sys->clip_system);
#endif
  return res;
}

NoteClipHandle ui_duplicate_timeline_note_clip(TimelineSystem* sys,
                                               TimelineNoteClipTrackHandle track_handle,
                                               NoteClipHandle src_handle) {
  auto track_clip = ui_find_track_and_clip(sys, track_handle, src_handle);

  auto new_beg = track_clip.clip->span.end(reference_time_signature().numerator);
  ScoreRegion new_span{new_beg, track_clip.clip->span.size};

  auto dst_clip = ui_clone_clip(sys->clip_system, src_handle);
  ui_set_clip_span(sys->clip_system, dst_clip, new_span);
  reconcile_new_clip_span(track_clip.track, sys->clip_system, new_span, NullOpt{});
  add_clip(track_clip.track, sys->clip_system, dst_clip);
  sys->note_clip_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track_clip.track->clips, sys->clip_system);
#endif
  return dst_clip;
}

NoteClipHandle ui_paste_timeline_note_clip_at_end(
  TimelineSystem* sys, TimelineNoteClipTrackHandle dst_track, NoteClipHandle src_clip) {
  //
  ScoreCursor beg = ui_get_track_span_end(sys, dst_track);
  ScoreCursor size = ui_read_clip(sys->clip_system, src_clip)->span.size;
  return ui_paste_timeline_note_clip(sys, dst_track, src_clip, ScoreRegion{beg, size});
}

NoteClipHandle ui_paste_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle dst_track,
                                           NoteClipHandle src_clip, ScoreRegion dst_clip_span) {
  //  @NOTE: Don't use `ui_find_track_and_clip` here, src_clip might come from a different track.
  assert(ui_is_clip(sys->clip_system, src_clip));
  auto* track = ui_find_track(sys, dst_track);
  assert(track);

  auto dst_clip = ui_clone_clip(sys->clip_system, src_clip);
  ui_set_clip_span(sys->clip_system, dst_clip, dst_clip_span);
  reconcile_new_clip_span(track, sys->clip_system, dst_clip_span, NullOpt{});
  add_clip(track, sys->clip_system, dst_clip);
  sys->note_clip_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips, sys->clip_system);
#endif
  return dst_clip;
}

void ui_set_timeline_note_clip_span(TimelineSystem* sys, TimelineNoteClipTrackHandle track_handle,
                                    NoteClipHandle clip_handle, ScoreRegion span) {
  assert(is_valid_clip_span(span));
  auto track_clip = ui_find_track_and_clip(sys, track_handle, clip_handle);

  ui_set_clip_span(sys->clip_system, clip_handle, span);
  //  Ignore the modified clip.
  auto skip = Optional<NoteClipHandle>(clip_handle);
  reconcile_new_clip_span(track_clip.track, sys->clip_system, span, skip);
  sort_clip_spans(track_clip.track->clips, sys->clip_system);
  sys->note_clip_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track_clip.track->clips, sys->clip_system);
#endif
}

void ui_destroy_timeline_note_clip(TimelineSystem* sys, TimelineNoteClipTrackHandle track_handle,
                                   NoteClipHandle clip_handle) {
  auto* track = ui_find_track(sys, track_handle);
  auto it = std::find(track->clips.begin(), track->clips.end(), clip_handle);
  assert(it != track->clips.end());
  track->clips.erase(it);
  ui_destroy_clip(sys->clip_system, clip_handle);
  sys->note_clip_tracks.modified = true;
#ifdef GROVE_DEBUG
  validate_clip_spans(track->clips, sys->clip_system);
#endif
}

void ui_set_track_loop_region(TimelineSystem* sys, TimelineNoteClipTrackHandle handle,
                              ScoreRegion region) {
  auto* track = ui_find_track(sys, handle);
  assert(is_valid_loop_region(region));
  track->loop_region = region;
  sys->note_clip_tracks.modified = true;
}

const TimelineNoteClipTrack* ui_read_note_clip_track(const TimelineSystem* system,
                                                     TimelineNoteClipTrackHandle handle) {
  return ui_find_track(system, handle);
}

ScoreCursor ui_get_track_span_end(const TimelineSystem* sys,
                                  TimelineNoteClipTrackHandle track_handle) {
  auto* track = ui_find_track(sys, track_handle);
  assert(track);
  if (track->clips.empty()) {
    return {};
  } else {
    const double num = reference_time_signature().numerator;
    return ui_read_clip(sys->clip_system, track->clips.back())->span.end(num);
  }
}

bool ui_maybe_insert_recorded_note(TimelineSystem* sys, TimelineNoteClipTrackHandle track_handle,
                                   ClipNote note) {
  auto* track = ui_find_track(sys, track_handle);
  assert(track);

  const double num = reference_time_signature().numerator;
  if (track->loop_region) {
    note.span.begin = track->loop_region.value().loop(note.span.begin, num);
  }

  int handle_index{};
  auto* clip = find_clip_containing_cursor(
    sys->clip_system,
    note.span.begin,
    reference_time_signature().beats_per_measure(),
    track->clips.data(), int(track->clips.size()), &handle_index);

  if (clip) {
    note.span.begin.wrapped_sub_cursor(clip->span.begin, num);
    ui_add_note(sys->clip_system, track->clips[handle_index], note);
    return true;
  } else {
    return false;
  }
}

ScoreCursor TimelineAudioTrack::latest_span_end() const {
  if (clips.empty()) {
    return {};
  } else {
    return clips.back().span.end(reference_time_signature().numerator);
  }
}

TimelineAudioTrackNode::TimelineAudioTrackNode(TimelineSystem* timeline_system,
                                               TimelineAudioTrackHandle handle,
                                               int num_output_channels) :
  timeline_system{timeline_system},
  track_handle{handle} {
  //
  for (int i = 0; i < num_output_channels; i++) {
    output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
  }
}

void TimelineAudioTrackNode::process(const AudioProcessData&, const AudioProcessData& out,
                                     AudioEvents*, const AudioRenderInfo& info) {
  auto* track = render_get_timeline_audio_track(timeline_system, track_handle);
  if (!track) {
    return;
  }

  TimelineAudioTrackRenderContext context{};
  context.render_info = &info;
  context.transport = render_get_transport(timeline_system);
  context.buffer_store = render_get_buffer_store(timeline_system);

  const auto playback_info = get_transport_playback_info(
    context.transport, context.render_info->sample_rate);

  render_timeline_audio_track(*track, out, playback_info, context);
}

InputAudioPorts TimelineAudioTrackNode::inputs() const {
  return {};
}

OutputAudioPorts TimelineAudioTrackNode::outputs() const {
  return output_ports;
}

TimelineNoteClipTrackNode::TimelineNoteClipTrackNode(
  const TimelineSystem* system, TimelineNoteClipTrackHandle handle) :
  system{system}, track_handle{handle} {
  //
  output_ports.push_back(OutputAudioPort{BufferDataType::MIDIMessage, this, 0});
}

InputAudioPorts TimelineNoteClipTrackNode::inputs() const {
  return {};
}

OutputAudioPorts TimelineNoteClipTrackNode::outputs() const {
  return output_ports;
}

void TimelineNoteClipTrackNode::process(const AudioProcessData&, const AudioProcessData& out,
                                        AudioEvents*, const AudioRenderInfo& info) {
  auto* track = render_get_timeline_note_clip_track(system, track_handle);
  if (!track) {
    return;
  }

  auto maybe_messages = midi::render_read_stream_messages(
    system->midi_message_stream_system, MIDIMessageStreamHandle{track->midi_stream_id});
  if (!maybe_messages) {
    return;
  }
  auto& src_messages = maybe_messages.value();

  assert(src_messages.size() == info.num_frames);
  assert(out.descriptors.size() == 1 && out.descriptors[0].is_midi_message());

  auto& desc = out.descriptors[0];
  for (int i = 0; i < info.num_frames; i++) {
    auto message = src_messages[i];
    desc.write(out.buffer.data, i, &message);
  }
}

GROVE_NAMESPACE_END
