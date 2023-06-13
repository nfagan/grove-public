#pragma once

#include "types.hpp"
#include "grove/common/identifier.hpp"

namespace grove {

struct ArpeggiatorSystem;
struct MIDIMessageStreamSystem;
struct PitchSampleSetGroupHandle;
struct PitchSamplingSystem;
class Transport;

enum class ArpeggiatorSystemPitchMode {
  Random,
  CycleUp,
  RandomFromPitchSampleSet,
  CycleUpFromPitchSampleSet,
  SIZE
};

enum class ArpeggiatorSystemDurationMode {
  Random,
  Quarter,
  Eighth,
  Sixteenth,
  SIZE
};

struct ArpeggiatorInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(ArpeggiatorInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct ReadArpeggiatorState {
  ArpeggiatorSystemPitchMode pitch_mode;
  ArpeggiatorSystemDurationMode duration_mode;
  int num_slots_active;
};

namespace arp {

ArpeggiatorSystem* get_global_arpeggiator_system();

void render_begin_process(ArpeggiatorSystem* sys, const AudioRenderInfo& info);

uint8_t get_midi_source_id();
void ui_initialize(
  ArpeggiatorSystem* sys, MIDIMessageStreamSystem* midi_message_stream_sys,
  PitchSamplingSystem* pitch_sampling_sys, const Transport* transport);
void ui_update(ArpeggiatorSystem* sys);

void ui_set_note_sampling_parameters(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp,
  const PitchClass* pcs, uint8_t num_pcs, const int8_t* octs, uint8_t num_octs);
void ui_set_note_cycling_parameters(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, const MIDINote* notes, uint8_t num_notes,
  uint8_t st_step, uint8_t num_steps);
void ui_set_pitch_sample_set_group(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, const PitchSampleSetGroupHandle& pss);
void ui_set_pitch_mode(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, ArpeggiatorSystemPitchMode mode);
void ui_set_duration_mode(
  ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, ArpeggiatorSystemDurationMode mode);
void ui_set_num_active_slots(ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle arp, uint8_t num);

ArpeggiatorInstanceHandle ui_create_arpeggiator(
  ArpeggiatorSystem* sys, uint32_t midi_message_stream);
void ui_destroy_arpeggiator(ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle inst);

int ui_get_num_instances(const ArpeggiatorSystem* sys);
ArpeggiatorInstanceHandle ui_get_ith_instance(const ArpeggiatorSystem* sys, int i);
ReadArpeggiatorState ui_read_state(const ArpeggiatorSystem* sys, ArpeggiatorInstanceHandle inst);

}

}