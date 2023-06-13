#include "EditorGUI.hpp"
#include "../editor/editor.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

using namespace editor;

EditorGUI::UpdateResult editor::EditorGUI::render(const Editor& editor) {
  UpdateResult result{};
  ImGui::Begin("EditorGUI");

  bool tform_editor_enabled = editor.transform_editor_enabled;
  if (ImGui::Checkbox("TransformEditorEnabled", &tform_editor_enabled)) {
    result.transform_editor_enabled = tform_editor_enabled;
  }

  ImGui::Text("NumTransformEditorInstances: %d", editor.transform_editor.num_instances());

  if (ImGui::Button("Close")) {
    result.close = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
