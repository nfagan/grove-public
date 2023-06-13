#pragma once

#include "types.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove {

struct MIDIMessageStreamSystem;

struct QuantizedTriggeredNotes;
class Transport;

struct QuantizedTriggeredNoteMessage {
  uint8_t track;
  uint8_t note;
  int frame;
  bool on;
};

struct QuantizedTriggeredNotesUpdateResult {
  ArrayView<const uint64_t> newly_played;
};

struct QuantizedTriggeredNotesStats {
  int num_ui_pending_feedback;
  int max_num_note_messages;
  int num_note_feedbacks_created;
};

namespace qtn {

QuantizedTriggeredNotes* get_global_quantized_triggered_notes();

uint64_t ui_trigger(
  QuantizedTriggeredNotes* notes, uint32_t track, MIDINote note,
  audio::Quantization next_quantum, double beat_duration);

void ui_initialize(QuantizedTriggeredNotes* notes, const Transport* transport);
QuantizedTriggeredNotesUpdateResult ui_update(QuantizedTriggeredNotes* notes);

QuantizedTriggeredNotesStats ui_get_stats(const QuantizedTriggeredNotes* notes);
uint8_t ui_get_midi_source_id();

void begin_process(
  QuantizedTriggeredNotes* notes, MIDIMessageStreamSystem* midi_message_stream_sys,
  const AudioRenderInfo& info);

}

}