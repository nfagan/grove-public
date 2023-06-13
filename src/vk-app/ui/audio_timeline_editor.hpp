#pragma once

#include "audio_editors_common.hpp"

namespace grove::ui {

void prepare_audio_timeline_editor(const AudioEditorCommonContext& common_context);
void evaluate_audio_timeline_editor(const AudioEditorCommonContext& common_context);
void render_audio_timeline_editor(const AudioEditorCommonContext& common_context);
void destroy_audio_timeline_editor();

}