#pragma once

#include "types.hpp"

namespace grove {
class Transport;
}

namespace grove::metronome {

struct Metronome;

Metronome* get_global_metronome();
void ui_initialize(Metronome* metronome, const Transport* transport);
void ui_set_enabled(Metronome* metronome, bool enable);
void ui_toggle_enabled(Metronome* metronome);
bool ui_is_enabled(const Metronome* metronome);

void render_process(Metronome* metronome, Sample* dst, const AudioRenderInfo& info);

}