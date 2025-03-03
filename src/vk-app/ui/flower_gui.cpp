#include "flower_gui.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "ui_common.hpp"
#include "../procedural_flower/ProceduralFlowerComponent.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct FlowerGUIData {
  BoxDrawList box_draw_list;
};

void do_create_patch(void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto& comp = ctx->procedural_flower_component;
  comp.add_patch_at_cursor_position();
}

void do_create_patches_around_world(void* context) {
  auto* ctx = static_cast<WorldGUIContext*>(context);
  auto& comp = ctx->procedural_flower_component;
  comp.add_patches_around_world();
}

struct {
  FlowerGUIData data;
} globals;

} //  anon

void gui::clear_flower_gui() {
  globals.data.box_draw_list.clear();
}

void gui::prepare_flower_gui(layout::Layout* layout, int container, elements::Elements& elements,
                             const WorldGUIContext& context) {
  auto& data = globals.data;

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();
  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int section0 = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  layout::begin_group(layout, section0, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int row0 = prepare_row(layout, line_h, 0);
  layout::end_group(layout);

  {
    const int nb = 2;
    int buttons[nb];
    const char* texts[nb]{"create one", "create many"};
    const decltype(&do_create_patch) funcs[nb]{&do_create_patch, &do_create_patches_around_world};

    layout::begin_group(layout, row0, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    for (int i = 0; i < nb; i++) {
      const float bw = ui::font_sequence_width_ascii(text_font, texts[i], font_size, 4.0f);
      buttons[i] = prepare_button(elements, layout, {1, bw, bw}, line_h, false, funcs[i]);
      if (i + 1 < nb) {
        layout::set_box_margin(layout, buttons[i], 0, 0, 8, 0);
      }
    }
    layout::end_group(layout);

    for (int i = 0; i < nb; i++) {
      draw_label(context.render_data, layout::read_box(layout, buttons[i]), texts[i], text_font, font_size, Vec3f{}, 4, false);
      draw_box(data.box_draw_list, layout, buttons[i], ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f));
    }
  }
}

void gui::render_flower_gui(const layout::Layout* layout, const WorldGUIContext& context) {
  auto& draw_list = globals.data.box_draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, layout);
  gui::push_draw_list(context.render_data, draw_list);
}

GROVE_NAMESPACE_END
