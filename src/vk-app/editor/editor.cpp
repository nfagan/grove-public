#include "editor.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"

GROVE_NAMESPACE_BEGIN

using namespace editor;

namespace {

void render_transform_editor(UIRenderer& ui_renderer,
                             const UIRenderer::DrawContext& context,
                             const TransformEditor::Instances& instances,
                             bool has_active) {
  for (const auto& [id, inst] : instances) {
    for (uint32_t i = 0; i < inst.num_selectables; i++) {
      auto* monitorable = inst.cursor_monitorables[i];
      Vec3f color_scale{1.0f};
      if (has_active) {
        if (inst.active_plane_index && inst.active_plane_index.value() == i) {
          color_scale = Vec3f{0.75f};
        }
      } else if (monitorable->get_state().is_over()) {
        color_scale = Vec3f{0.75f};
      }
      if (!inst.drawable_info[i].disabled) {
        ui_renderer.draw_cube(
          context,
          inst.transform->get_current().translation,
          inst.drawable_info[i].scale,
          inst.drawable_info[i].color * color_scale);
      }
    }
  }
}

} //  anon

void editor::initialize(Editor* editor, const EditorInitInfo& info) {
  editor->ui_renderer.initialize({
    info.context,
    info.shape_renderer
  });
  editor->transform_system = info.transform_system;
}

editor::TransformEditorHandle editor::create_transform_editor(
  editor::Editor* editor, transform::TransformInstance* tform, const grove::Vec3f& offset) {
  //
  return editor->transform_editor.create_instance(
    tform, *editor->transform_system, editor->cursor_monitor, offset);
}

void editor::update(Editor* editor, const EditorUpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("editor/update");
  (void) profiler;
  UIRenderer::DrawContext draw_context{
    info.shape_renderer_context,
    info.shape_renderer
  };
  editor->ui_renderer.begin_update(draw_context);
  editor->cursor_monitor.update(info.cursor_ray, info.cursor_down, info.cursor_over_gui_window);
  if (editor->transform_editor_enabled) {
    editor->transform_editor.update({
      info.cursor_ray,
      info.cursor_down,
      editor->cursor_monitor
    });
    render_transform_editor(
      editor->ui_renderer,
      draw_context,
      editor->transform_editor.read_instances(),
      editor->transform_editor.has_active_instance());
  }
}

void editor::on_gui_update(Editor* editor, const EditorGUI::UpdateResult& gui_res) {
  if (gui_res.transform_editor_enabled) {
    editor->transform_editor_enabled = gui_res.transform_editor_enabled.value();
  }
}

GROVE_NAMESPACE_END
