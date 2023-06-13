#include "MIDIMessageStreamSystem.hpp"
#include "NoteNumberSet.hpp"
#include "NoteQueue.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Handshake.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/RingBuffer.hpp"
#include <vector>
#include <memory>

GROVE_NAMESPACE_BEGIN

enum class CommandType {
  SetSourceMask = 0,
  SetNoteOnsetsMask
};

struct SetSourceMask {
  uint8_t source;
  bool enable;
};

struct SetNoteOnsetsMask {
  uint8_t source;
  bool enable;
};

struct Command {
  CommandType type;
  MIDIMessageStreamHandle target;
  union {
    SetSourceMask set_source_mask;
    SetNoteOnsetsMask set_note_onsets_mask;
  };
};

struct StreamSourceMask {
  void add(uint8_t source) {
    sources[source] = true;
  }

  void remove(uint8_t source) {
    sources[source] = false;
  }

  bool contains(uint8_t source) const {
    return sources[source];
  }

  std::bitset<256> sources{};
};

struct NoteOnsetBits {
  std::bitset<256> bits{};
};

struct StreamNoteOnsets {
  void maybe_mark_onset(uint8_t src, uint8_t note) {
    if (enabled_for_source[src]) {
      onsets.bits[note] = true;
      any = true;
    }
  }
  void clear_onsets() {
    onsets = {};
    any = false;
  }
  void enable_source(uint8_t src) {
    enabled_for_source[src] = true;
  }
  void disable_source(uint8_t src) {
    enabled_for_source[src] = false;
  }

  NoteOnsetBits onsets{};
  std::bitset<256> enabled_for_source{};
  bool any{};
};

struct StreamNoteSources {
  bool is_on(uint8_t note) const {
    return on[note];
  }

  void set_note_off(uint8_t note) {
    assert(on[note]);
    on[note] = false;
  }

  void set_note_on(uint8_t note, uint8_t source, uint8_t channel) {
    assert(!on[note]);
    on[note] = true;
    sources[note] = source;
    channels[note] = channel;
  }

  bool is_source(uint8_t note, uint8_t source) const {
    return sources[note] == source;
  }

  uint8_t get_channel(uint8_t note) const {
    return channels[note];
  }

  std::bitset<256> on{};
  uint8_t sources[256]{};
  uint8_t channels[256]{};
};

struct Stream {
  void reserve_output_messages(int num_frames) {
    output_messages.resize(num_frames);
  }

  void clear_output_messages() {
    std::fill(output_messages.begin(), output_messages.end(), MIDIMessage{});
  }

  MIDIMessageStreamHandle handle{};
  NoteQueue<MIDIStreamMessage, 1024> pending_messages;
  uint32_t pending_message_end{};
  DynamicArray<MIDIMessage, 256> output_messages;
  StreamNoteSources note_sources;
  StreamNoteOnsets note_onsets;

  StreamSourceMask render_source_mask;
  RingBuffer<Command, 8> commands_from_ui;
  RingBuffer<NoteOnsetBits, 2> note_onset_feedback_to_ui;

  uint32_t max_num_pending_messages{};
};

using StreamVec = std::vector<std::shared_ptr<Stream>>;

struct Streams {
  Optional<int> ui_find_stream_index(MIDIMessageStreamHandle handle) const {
    int ind{};
    for (auto& stream : *streams0) {
      if (stream->handle == handle) {
        return Optional<int>(ind);
      }
      ind++;
    }
    return NullOpt{};
  }

  void destroy(MIDIMessageStreamHandle handle) {
    auto it = streams0->begin();
    while (it != streams0->end()) {
      if ((*it)->handle == handle) {
        streams0->erase(it);
        modified = true;
        return;
      }
      ++it;
    }
    assert(false);
  }

  std::unique_ptr<StreamVec> streams0;
  std::unique_ptr<StreamVec> streams1;
  std::unique_ptr<StreamVec> streams2;
  bool modified{};
};

struct UIStreamState {
  StreamSourceMask enabled_sources;
};

struct MIDIMessageStreamSystem {
  std::atomic<bool> initialized{};
  bool began_process{};
  int num_frames_reserved{};

  Streams streams;
  StreamVec* render_streams{};
  Handshake<StreamVec*> handoff_streams;
  DynamicArray<Command, 16> pending_commands;
  DynamicArray<UIStreamState, 16> ui_stream_states;

  std::atomic<uint32_t> max_num_pending_messages_across_streams{};

  DynamicArray<MIDIStreamNoteOnsetFeedback, 64> latest_feedback_note_onsets;
  uint32_t max_num_feedback_note_onsets{};

  uint32_t next_handle_id{1};
};

namespace {

Command make_empty_command(CommandType type, MIDIMessageStreamHandle target) {
  Command cmd{};
  cmd.type = type;
  cmd.target = target;
  return cmd;
}

Streams make_streams() {
  Streams result;
  result.streams0 = std::make_unique<StreamVec>();
  result.streams1 = std::make_unique<StreamVec>();
  result.streams2 = std::make_unique<StreamVec>();
  return result;
}

Stream* find_stream(StreamVec& v, MIDIMessageStreamHandle handle) {
  for (auto& el : v) {
    if (el->handle == handle) {
      return el.get();
    }
  }
  return nullptr;
}

bool less_message(const MIDIStreamMessage& a, const MIDIStreamMessage& b) {
  if (a.frame != b.frame) {
    return a.frame < b.frame;
  }

  const uint8_t na = a.message.note_number();
  const uint8_t nb = b.message.note_number();
  if (na != nb) {
    return na < nb;
  }

  if (a.message.is_note_off() && b.message.is_note_on()) {
    //  Same frame and same note number -> prefer the note-off event first.
    return true;
  }

  //  @TODO: This is really only OK because we don't generate other types of MIDI messages yet
  //  besides on and off. Otherwise, we'd prefer to keep messages in a consistent order.
  //  Don't care otherwise.
  return false;
}

void apply_from_command(StreamSourceMask& source_mask, const SetSourceMask& set) {
  if (set.enable) {
    source_mask.add(set.source);
  } else {
    source_mask.remove(set.source);
  }
}

void apply_commands(Stream* stream) {
  int nc = stream->commands_from_ui.size();
  for (int i = 0; i < nc; i++) {
    Command cmd = stream->commands_from_ui.read();
    switch (cmd.type) {
      case CommandType::SetSourceMask: {
        const SetSourceMask& set = cmd.set_source_mask;
        apply_from_command(stream->render_source_mask, set);
        break;
      }
      case CommandType::SetNoteOnsetsMask: {
        const SetNoteOnsetsMask& set = cmd.set_note_onsets_mask;
        if (set.enable) {
          stream->note_onsets.enable_source(set.source);
        } else {
          stream->note_onsets.disable_source(set.source);
        }
        break;
      }
    }
  }
}

void write_messages(Stream* stream, int num_frames) {
  auto& src_messages = stream->pending_messages;
  auto& dst_messages = stream->output_messages;
  auto& note_sources = stream->note_sources;
  auto& note_onsets = stream->note_onsets;

  if (!src_messages.empty()) {
    for (int i = 0; i < num_frames; i++) {
      auto* message = src_messages.peek_front();
      if (!message || message->frame < i) {
        continue;
      }

      const uint8_t note_num = message->message.note_number();
      if (message->message.is_note_off()) {
        if (note_sources.is_on(note_num)) {
          //  common case: turning off a note that was previously on.
          dst_messages[i] = message->message;
          note_sources.set_note_off(note_num);
        } else {
          //  nothing to do in this case.
        }
        //  process the next message
        (void) src_messages.pop_front();
      } else if (message->message.is_note_on()) {
        if (note_sources.is_on(note_num)) {
          //  This note is already playing, write a note-off message first.
          dst_messages[i] = MIDIMessage::make_note_off(
            note_sources.get_channel(note_num), note_num, 0);
          note_sources.set_note_off(note_num);
          //  Wait for the next frame to trigger the note-on message.
        } else {
          note_sources.set_note_on(note_num, message->source_id, message->message.channel());
          note_onsets.maybe_mark_onset(message->source_id, note_num);
          dst_messages[i] = message->message;
          (void) src_messages.pop_front();
        }
      } else {
        GROVE_LOG_SEVERE_CAPTURE_META(
          "MIDIMessage is not a note on or off message - may not be properly be handled currently.",
          "MIDIMessageStreamSystem");
        //  No special cases for other message types atm.
        dst_messages[i] = message->message;
        (void) src_messages.pop_front();
      }
    }
  }

  src_messages.erase_to_head();
  //  Some messages are left over and should be played as soon as possible in the next block.
  if (!src_messages.empty()) {
    for (auto& message : src_messages) {
      message.frame = 0;
    }
  }
}

void ui_send_commands(MIDIMessageStreamSystem* sys) {
  const int np = int(sys->pending_commands.size());
  for (int i = 0; i < np; i++) {
    auto& cmd = sys->pending_commands[0];
    auto* stream = find_stream(*sys->streams.streams0, cmd.target);
    if (!stream || stream->commands_from_ui.maybe_write(cmd)) {
      sys->pending_commands.erase(sys->pending_commands.begin());
    } else {
      break;
    }
  }
}

ArrayView<const MIDIStreamNoteOnsetFeedback>
ui_gather_feedback_note_onsets(MIDIMessageStreamSystem* sys) {
  //
  sys->latest_feedback_note_onsets.clear();

  for (auto& stream : *sys->streams.streams0) {
    const auto stream_handle = stream->handle;
    //  Only read 1 feedback set per frame so that there is no possibility of duplicate notes
    //  within a stream. This is not technically necessary so long as the ring buffer capacity is
    //  2, since in that case there can only ever be 1 feedback item written at a time.
    int nf = std::min(1, stream->note_onset_feedback_to_ui.size());
    for (int i = 0; i < nf; i++) {
      auto onsets = stream->note_onset_feedback_to_ui.read();
      for (int j = 0; j < 256; j++) {
        if (onsets.bits[j]) {
          MIDIStreamNoteOnsetFeedback onset{};
          onset.stream = stream_handle;
          onset.note_number = uint8_t(j);
          sys->latest_feedback_note_onsets.push_back(onset);
        }
      }
    }
  }

  sys->max_num_feedback_note_onsets = std::max(
    sys->max_num_feedback_note_onsets, uint32_t(sys->latest_feedback_note_onsets.size()));

  return make_view(sys->latest_feedback_note_onsets);
}

void set_ui_stream_state(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, const Command& cmd) {
  //
  if (auto ind = sys->streams.ui_find_stream_index(stream)) {
    auto& state = sys->ui_stream_states[ind.value()];
    switch (cmd.type) {
      case CommandType::SetSourceMask: {
        apply_from_command(state.enabled_sources, cmd.set_source_mask);
        break;
      }
      case CommandType::SetNoteOnsetsMask: {
        break;
      }
      default: {
        assert(false);
      }
    }
  }
}

struct {
  MIDIMessageStreamSystem sys;
} globals;

} //  anon

MIDIMessageStreamSystem* midi::get_global_midi_message_stream_system() {
  return &globals.sys;
}

void midi::render_begin_process(MIDIMessageStreamSystem* sys, const AudioRenderInfo& info) {
  sys->began_process = false;

  if (!sys->initialized.load()) {
    return;
  }

  if (auto rd = read(&sys->handoff_streams)) {
    sys->render_streams = rd.value();
  }

  auto& streams = *sys->render_streams;
  uint32_t max_num_messages{};
  for (auto& stream : streams) {
    stream->reserve_output_messages(info.num_frames);
    stream->clear_output_messages();
    stream->pending_message_end = uint32_t(stream->pending_messages.size());
    apply_commands(stream.get());
    max_num_messages = std::max(max_num_messages, stream->max_num_pending_messages);
  }

  sys->began_process = true;
  sys->num_frames_reserved = info.num_frames;
  sys->max_num_pending_messages_across_streams.store(max_num_messages);
}

void midi::render_end_process(MIDIMessageStreamSystem* sys) {
  if (!sys->began_process) {
    return;
  }

  for (auto& stream : *sys->render_streams) {
    StreamNoteOnsets& onsets = stream->note_onsets;
    if (onsets.any && stream->note_onset_feedback_to_ui.maybe_write(onsets.onsets)) {
      onsets.clear_onsets();
    }
  }
}

bool midi::render_broadcast_messages(
  MIDIMessageStreamSystem* sys, const MIDIStreamMessage* messages, int num_messages) {
  //
  if (!sys->began_process) {
    return false;
  }

  bool all_ok{true};
  for (auto& stream : *sys->render_streams) {
    if (!midi::render_push_messages(sys, stream->handle, messages, num_messages)) {
      all_ok = false;
    }
  }

  return all_ok;
}

Optional<MIDIMessageStreamHandle> midi::render_get_ith_stream(MIDIMessageStreamSystem* sys, int i) {
  if (!sys->began_process || i >= int(sys->render_streams->size())) {
    return NullOpt{};
  }

  return Optional<MIDIMessageStreamHandle>((*sys->render_streams)[i]->handle);
}

bool midi::render_can_write_to_stream(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle handle) {
  if (!sys->began_process) {
    return false;
  }

  for (auto& s : *sys->render_streams) {
    if (s->handle == handle) {
      return true;
    }
  }

  return false;
}

bool midi::render_push_messages(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream_handle,
  const MIDIStreamMessage* messages, int num_messages) {
  //
  if (!sys->began_process) {
    return false;
  }

  auto* stream = find_stream(*sys->render_streams, stream_handle);
  if (!stream) {
    return false;
  }

  for (int i = 0; i < num_messages; i++) {
    //  If the message is from a source that is masked-in, then append it unconditionally.
    //  Otherwise, allow a note-off message when a note-on message was previously issued from this
    //  source.
    const MIDIStreamMessage& msg = messages[i];
    const bool do_append = stream->render_source_mask.contains(msg.source_id) ||
      (msg.message.is_note_off() &&
      stream->note_sources.is_on(msg.message.note_number()) &&
      stream->note_sources.is_source(msg.message.note_number(), msg.source_id));
    if (do_append) {
      stream->pending_messages.push_back(msg);
    }
  }

  return true;
}

void midi::render_write_streams(MIDIMessageStreamSystem* sys) {
  if (!sys->began_process) {
    return;
  }

  auto& streams = *sys->render_streams;
  for (auto& stream : streams) {
    assert(stream->pending_message_end <= uint32_t(stream->pending_messages.size()));

    stream->max_num_pending_messages = std::max(
      stream->max_num_pending_messages, uint32_t(stream->pending_messages.size()));

    auto* sort_beg = stream->pending_messages.begin() + stream->pending_message_end;
    auto* sort_end = stream->pending_messages.end();
    std::sort(sort_beg, sort_end, less_message);

    write_messages(stream.get(), sys->num_frames_reserved);
  }
}

Optional<ArrayView<const MIDIMessage>> midi::render_read_stream_messages(
  const MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream_handle) {
  //
  if (!sys->began_process) {
    return NullOpt{};
  }

  auto* stream = find_stream(*sys->render_streams, stream_handle);
  if (!stream) {
    return NullOpt{};
  }

  auto view = make_view(stream->output_messages);
  return Optional<decltype(view)>(view);
}

MIDIMessageStreamHandle midi::ui_create_stream(MIDIMessageStreamSystem* sys) {
  auto stream = std::make_shared<Stream>();
  auto handle = MIDIMessageStreamHandle{sys->next_handle_id++};
  stream->handle = handle;

  sys->streams.streams0->push_back(std::move(stream));
  sys->ui_stream_states.emplace_back();
  sys->streams.modified = true;

  return handle;
}

void midi::ui_destroy_stream(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream) {
  auto ind = sys->streams.ui_find_stream_index(stream);
  if (ind) {
    sys->ui_stream_states.erase(sys->ui_stream_states.begin() + ind.value());
  }
  sys->streams.destroy(stream);
}

void midi::ui_initialize(MIDIMessageStreamSystem* sys) {
  assert(!sys->initialized.load());
  sys->streams = make_streams();
  sys->render_streams = sys->streams.streams2.get();
  sys->initialized.store(true);
}

MIDIMessageStreamSystemUpdateResult midi::ui_update(MIDIMessageStreamSystem* sys) {
  MIDIMessageStreamSystemUpdateResult result{};

  ui_send_commands(sys);

  if (sys->streams.modified && !sys->handoff_streams.awaiting_read) {
    *sys->streams.streams1 = *sys->streams.streams0;
    publish(&sys->handoff_streams, sys->streams.streams1.get());
    sys->streams.modified = false;
  }

  if (sys->handoff_streams.awaiting_read && acknowledged(&sys->handoff_streams)) {
    std::swap(sys->streams.streams1, sys->streams.streams2);
  }

  result.note_onsets = ui_gather_feedback_note_onsets(sys);
  return result;
}

bool midi::ui_is_source_enabled(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id) {
  //
  if (auto ind = sys->streams.ui_find_stream_index(stream)) {
    return sys->ui_stream_states[ind.value()].enabled_sources.contains(id);
  } else {
    return false;
  }
}

void midi::ui_enable_source(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream_handle, uint8_t id) {
  //
  Command cmd = make_empty_command(CommandType::SetSourceMask, stream_handle);
  auto& set = cmd.set_source_mask;
  set.source = id;
  set.enable = true;
  sys->pending_commands.push_back(cmd);
  set_ui_stream_state(sys, stream_handle, cmd);
}

void midi::ui_disable_source(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id) {
  //
  Command cmd = make_empty_command(CommandType::SetSourceMask, stream);
  auto& set = cmd.set_source_mask;
  set.source = id;
  set.enable = false;
  sys->pending_commands.push_back(cmd);
  set_ui_stream_state(sys, stream, cmd);
}

void midi::ui_enable_source_note_onset_feedback(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id) {
  //
  Command cmd = make_empty_command(CommandType::SetNoteOnsetsMask, stream);
  auto& set = cmd.set_note_onsets_mask;
  set.source = id;
  set.enable = true;
  sys->pending_commands.push_back(cmd);
}

void midi::ui_set_source_enabled(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id, bool enable) {
  //
  if (enable) {
    ui_enable_source(sys, stream, id);
  } else {
    ui_disable_source(sys, stream, id);
  }
}

void midi::ui_disable_source_note_onset_feedback(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id) {
  //
  Command cmd = make_empty_command(CommandType::SetNoteOnsetsMask, stream);
  auto& set = cmd.set_note_onsets_mask;
  set.source = id;
  set.enable = false;
  sys->pending_commands.push_back(cmd);
}

MIDIMessageStreamSystemStats midi::ui_get_stats(const MIDIMessageStreamSystem* sys) {
  MIDIMessageStreamSystemStats result{};
  result.num_streams = int(sys->streams.streams0->size());
  result.num_pending_set_source_mask = int(sys->pending_commands.size());
  result.max_num_pending_messages_across_streams = int(
    sys->max_num_pending_messages_across_streams.load());
  result.max_num_feedback_note_onsets = int(sys->max_num_feedback_note_onsets);
  return result;
}

GROVE_NAMESPACE_END
