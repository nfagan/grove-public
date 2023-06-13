#include "debug_note_clip_state_machine.hpp"
#include "control_note_clip_state_machine.hpp"
#include "AudioComponent.hpp"
#include "grove/audio/cursor.hpp"
#include "grove/common/common.hpp"
#include <imgui.h>

GROVE_NAMESPACE_BEGIN

void debug::render_debug_note_clip_state_machine_gui(const DebugNotClipStateMachineContext& context) {
  ImGui::Begin("NoteClipStateMachine");

  auto* sys = context.audio_component.get_note_clip_state_machine_system();
  auto* clip_sys = context.audio_component.get_note_clip_system();
  auto* control_ncsm = &context.control_ncsm;

  const int nv = ncsm::ui_get_num_voices(sys);
  const int ns = ncsm::ui_get_num_sections(sys);

  for (int v = 0; v < nv; v++) {
    std::string id{"Voice"};
    id += std::to_string(v);
    if (ImGui::TreeNode(id.c_str())) {
      auto read_voice = ncsm::ui_read_voice(sys, v);
      auto& pos = read_voice.position;
      ImGui::Text("Section: %d", read_voice.section);
      ImGui::Text("Measure: %d; Beat: %0.3f", int(pos.measure), float(pos.beat));
      if (ns > 0 && ImGui::Button("Proceed")) {
        ncsm::ui_set_next_section_index(sys, v, (read_voice.section + 1) % ns);
      }

      auto control_voice = ncsm::read_voice(control_ncsm, v);
      const int num_ranges = ncsm::get_num_section_ranges(control_ncsm);
      if (ImGui::SliderInt("SectionRangeIndex", &control_voice.section_range_index, 0, num_ranges - 1)) {
        ncsm::set_section_range(control_ncsm, sys, v, control_voice.section_range_index);
      }

      ImGui::TreePop();
    }
  }

  for (int s = 0; s < ns; s++) {
    std::string id{"Section"};
    id += std::to_string(s);
    if (ImGui::TreeNode(id.c_str())) {
      auto read_section = ncsm::ui_read_section(sys, s);
      if (ui_is_clip(clip_sys, read_section.clip_handle)) {
        auto* clip = ui_read_clip(clip_sys, read_section.clip_handle);
        auto measure = int(clip->span.size.measure);
        auto beat = decode(encode(clip->span.size, QuantizedScoreCursor::Depth::D4));
        auto sixteenth = decode(encode(clip->span.size, QuantizedScoreCursor::Depth::D16));
        int dbeat = int(beat.beat);
        int dsix = int((sixteenth.beat - beat.beat) * 4.0f);
        bool changed{};
        if (ImGui::SliderInt("Measure", &measure, 0, 16)) {
          changed = true;
        }
        if (ImGui::SliderInt("Beat", &dbeat, 0, 3)) {
          changed = true;
        }
        if (ImGui::SliderInt("Sixteenth", &dsix, 0, 3)) {
          changed = true;
        }
        if (changed) {
          double new_beat = double(dbeat) + double(dsix) / 4.0;
          ScoreCursor new_span{measure, 0.0};
          new_span.wrapped_add_beats(new_beat, reference_time_signature().numerator);
          if (new_span > ScoreCursor{}) {
            ui_set_clip_span(clip_sys, read_section.clip_handle, ScoreRegion{{}, new_span});
          }
        }
      }
      ImGui::TreePop();
    }
  }

  ImGui::End();
}

GROVE_NAMESPACE_END
