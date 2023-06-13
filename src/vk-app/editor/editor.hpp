#pragma once

#include "cursor.hpp"
#include "render.hpp"
#include "transform_editor.hpp"
#include "../imgui/EditorGUI.hpp"

namespace grove::editor {

struct Editor {
  cursor::Monitor cursor_monitor;
  editor::UIRenderer ui_renderer;
  editor::TransformEditor transform_editor;
  transform::TransformSystem* transform_system{};
  bool transform_editor_enabled{true};
};

struct EditorUpdateInfo {
  const SimpleShapeRenderer::AddResourceContext& shape_renderer_context;
  SimpleShapeRenderer& shape_renderer;
  const Ray& cursor_ray;
  bool cursor_down;
  bool cursor_over_gui_window;
  bool accum_selections;
};

struct EditorInitInfo {
  transform::TransformSystem* transform_system;
  const SimpleShapeRenderer::AddResourceContext& context;
  SimpleShapeRenderer& shape_renderer;
};

editor::TransformEditorHandle create_transform_editor(
  Editor* editor, transform::TransformInstance* tform, const Vec3f& offset);
void initialize(Editor* editor, const EditorInitInfo& info);
void update(Editor* editor, const EditorUpdateInfo& info);
void on_gui_update(Editor* editor, const EditorGUI::UpdateResult& gui_res);

}