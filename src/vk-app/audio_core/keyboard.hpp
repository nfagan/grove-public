#pragma once

#include "grove/audio/types.hpp"
#include "grove/input/KeyTrigger.hpp"

namespace grove::audio {

using KeyPressNotes = DynamicArray<MIDINote, 15>;

KeyPressNotes gather_key_press_notes(const KeyTrigger::KeyState& pressed);
MIDINotes key_press_notes_to_midi_notes(const KeyPressNotes& key_press_notes, int octave);

}