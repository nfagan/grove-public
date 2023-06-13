#include "keyboard.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

MIDINotes audio::key_press_notes_to_midi_notes(const KeyPressNotes& key_press_notes, int octave) {
  MIDINotes notes{};
  for (auto note : key_press_notes) {
    note.octave = note.octave + int8_t(octave);
    notes.push_back(note);
  }
  return notes;
}

audio::KeyPressNotes audio::gather_key_press_notes(const KeyTrigger::KeyState& pressed) {
  KeyPressNotes notes;

  if (pressed.count(Key::A) > 0) {
    notes.push_back(MIDINote{PitchClass::C, 0, 127});
  }
  if (pressed.count(Key::W) > 0) {
    notes.push_back(MIDINote{PitchClass::Cs, 0, 127});
  }
  if (pressed.count(Key::S) > 0) {
    notes.push_back(MIDINote{PitchClass::D, 0, 127});
  }
  if (pressed.count(Key::E) > 0) {
    notes.push_back(MIDINote{PitchClass::Ds, 0, 127});
  }
  if (pressed.count(Key::D) > 0) {
    notes.push_back(MIDINote{PitchClass::E, 0, 127});
  }
  if (pressed.count(Key::F) > 0) {
    notes.push_back(MIDINote{PitchClass::F, 0, 127});
  }
  if (pressed.count(Key::T) > 0) {
    notes.push_back(MIDINote{PitchClass::Fs, 0, 127});
  }
  if (pressed.count(Key::G) > 0) {
    notes.push_back(MIDINote{PitchClass::G, 0, 127});
  }
  if (pressed.count(Key::Y) > 0) {
    notes.push_back(MIDINote{PitchClass::Gs, 0, 127});
  }
  if (pressed.count(Key::H) > 0) {
    notes.push_back(MIDINote{PitchClass::A, 0, 127});
  }
  if (pressed.count(Key::U) > 0) {
    notes.push_back(MIDINote{PitchClass::As, 0, 127});
  }
  if (pressed.count(Key::J) > 0) {
    notes.push_back(MIDINote{PitchClass::B, 0, 127});
  }
  if (pressed.count(Key::K) > 0) {
    notes.push_back(MIDINote{PitchClass::C, 1, 127});
  }
  if (pressed.count(Key::O) > 0) {
    notes.push_back(MIDINote{PitchClass::Cs, 1, 127});
  }
  if (pressed.count(Key::L) > 0) {
    notes.push_back(MIDINote{PitchClass::D, 1, 127});
  }

  return notes;
}

GROVE_NAMESPACE_END
