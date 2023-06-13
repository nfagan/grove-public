#pragma once

namespace grove::notes {

static constexpr int max_num_notes = 32;

int ui_get_note_set0(float sts[max_num_notes]);
int ui_get_note_set1(float sts[max_num_notes]);
int ui_get_note_set2(float sts[max_num_notes]);
int ui_get_note_set3(float sts[max_num_notes]);
int ui_get_pentatonic_major_note_set(float sts[max_num_notes]);

}