#pragma once

#include "audio_editors_common.hpp"

namespace grove::ui {

struct AudioEditorData {
  AudioEditorMode mode{};
  bool hidden{};
};

void maybe_cycle_mode(AudioEditorData& data, bool forwards);
void prepare_audio_editors(AudioEditorData& data, const AudioEditorCommonContext& context);
void evaluate_audio_editors(AudioEditorData& data, const AudioEditorCommonContext& context);
void render_audio_editors(AudioEditorData& data, const AudioEditorCommonContext& context);
void destroy_audio_editors(AudioEditorData& data);

}