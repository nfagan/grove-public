#include "audio_editors.hpp"
#include "audio_node_editor.hpp"
#include "audio_timeline_editor.hpp"
#include "audio_track_editor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void ui::maybe_cycle_mode(AudioEditorData& data, bool forwards) {
  if (!data.hidden) {
    const int num_modes = 3;
    if (forwards) {
      do {
        data.mode = AudioEditorMode((int(data.mode) + 1) % num_modes);
      } while (data.mode == AudioEditorMode::Timeline);
    } else {
      int mode = int(data.mode);
      data.mode = AudioEditorMode(mode == 0 ? num_modes - 1 : mode - 1);
    }
  }
}

void ui::prepare_audio_editors(AudioEditorData&, const AudioEditorCommonContext& context) {
  prepare_audio_node_editor(context);
  prepare_audio_timeline_editor(context);
  prepare_audio_track_editor(context);
}

void ui::evaluate_audio_editors(AudioEditorData&, const AudioEditorCommonContext& context) {
  evaluate_audio_node_editor(context);
  evaluate_audio_timeline_editor(context);
  evaluate_audio_track_editor(context);
}

void ui::render_audio_editors(AudioEditorData&, const AudioEditorCommonContext& context) {
  render_audio_node_editor(context);
  render_audio_timeline_editor(context);
  render_audio_track_editor(context);
}

void ui::destroy_audio_editors(AudioEditorData&) {
  destroy_audio_node_editor();
  destroy_audio_timeline_editor();
  destroy_audio_track_editor();
}

GROVE_NAMESPACE_END
