#pragma once

#include "audio_editors_common.hpp"

namespace grove::ui {

void prepare_audio_track_editor(const AudioEditorCommonContext& context);
void evaluate_audio_track_editor(const AudioEditorCommonContext& context);
void render_audio_track_editor(const AudioEditorCommonContext& context);
void destroy_audio_track_editor();

}