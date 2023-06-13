#pragma once

#include "types.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

struct MIDIMessageStreamSystem;

struct MIDIMessageStreamHandle {
  uint32_t id;
  GROVE_INTEGER_IDENTIFIER_EQUALITY(MIDIMessageStreamHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
};

struct MIDIMessageStreamSystemStats {
  int num_streams;
  int num_pending_set_source_mask;
  int max_num_pending_messages_across_streams;
  int max_num_feedback_note_onsets;
};

struct MIDIStreamMessage {
  int frame;
  uint8_t source_id;
  MIDIMessage message;
};

struct MIDIStreamNoteOnsetFeedback {
  MIDIMessageStreamHandle stream;
  uint8_t note_number;
};

struct MIDIMessageStreamSystemUpdateResult {
  ArrayView<const MIDIStreamNoteOnsetFeedback> note_onsets;
};

namespace midi {

MIDIMessageStreamSystem* get_global_midi_message_stream_system();

void render_begin_process(MIDIMessageStreamSystem* sys, const AudioRenderInfo& info);
void render_end_process(MIDIMessageStreamSystem* sys);

bool render_push_messages(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream,
  const MIDIStreamMessage* messages, int num_messages);
bool render_broadcast_messages(MIDIMessageStreamSystem* sys,
                               const MIDIStreamMessage* messages, int num_messages);
Optional<MIDIMessageStreamHandle> render_get_ith_stream(MIDIMessageStreamSystem* sys, int i);
bool render_can_write_to_stream(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle handle);

void render_write_streams(MIDIMessageStreamSystem* sys);

Optional<ArrayView<const MIDIMessage>>
render_read_stream_messages(const MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream);

void ui_initialize(MIDIMessageStreamSystem* sys);

MIDIMessageStreamHandle ui_create_stream(MIDIMessageStreamSystem* sys);
void ui_destroy_stream(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream);

[[nodiscard]] MIDIMessageStreamSystemUpdateResult ui_update(MIDIMessageStreamSystem* sys);
bool ui_is_source_enabled(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id);
void ui_enable_source(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id);
void ui_disable_source(MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id);
void ui_set_source_enabled(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id, bool enable);
void ui_enable_source_note_onset_feedback(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id);
void ui_disable_source_note_onset_feedback(
  MIDIMessageStreamSystem* sys, MIDIMessageStreamHandle stream, uint8_t id);
MIDIMessageStreamSystemStats ui_get_stats(const MIDIMessageStreamSystem* sys);

}

}