#include "screen0_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "grove/common/common.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/visual/image_process.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct Screen0GUIData {
  gui::BoxDrawList draw_list;
  gui::elements::Elements elements;
  gui::layout::Layout* layout{};
};

struct {
  Screen0GUIData data;
} globals;

void close_gui(void* context) {
  auto* ctx = static_cast<const Screen0GUIContext*>(context);
  ctx->gui_result->close_screen = true;
}

} //  anon

void gui::prepare_screen0_gui(const Screen0GUIContext& context) {
  auto& data = globals.data;

  if (!data.layout) {
    data.layout = gui::layout::create_layout(GROVE_SCREEN0_GUI_LAYOUT_ID);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.draw_list.clear();
  elements::begin_elements(&data.elements, GROVE_SCREEN0_GUI_LAYOUT_ID);

  if (context.hidden) {
    return;
  }

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();
  (void) text_font;

  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  auto fb_dims = context.container_dimensions;
  layout::set_root_dimensions(layout, fb_dims.x, fb_dims.y);

  layout::begin_group(layout, 0, layout::GroupOrientation::Row);
  int root = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  auto bg_color = image::srgb_to_linear(Vec3f{228.0f/255.0f, 191.0f/255.0f, 242.0f/255.0f});
  auto bg_desc = ui::make_render_quad_desc_style(bg_color);
  draw_box(data.draw_list, layout, root, bg_desc);

  auto button_row = [&](int row, const char* text, bool highlighted, elements::ClickCallback* cb) {
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f) * 4.0f;
    layout::begin_group(layout, row, layout::GroupOrientation::Col, 0, 0);
    int button = prepare_button(data.elements, layout, {1, bw, bw}, line_h, true, cb);
    layout::end_group(layout);

    const auto border_color = highlighted ? Vec3f{1.0f, 0.0f, 0.0f} : Vec3f{1.0f};
    const auto box_color = Vec3f{1.0f};
    draw_label(context.render_data, layout::read_box(layout, button), text, text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.draw_list, layout, button, ui::make_render_quad_desc_style(box_color, 0.0f, border_color));
  };

  button_row(root, "grove", false, close_gui);

  auto* cursor = &context.cursor_state;
  const auto* boxes = layout::read_box_slot_begin(layout);
  cursor::evaluate_boxes(cursor, GROVE_SCREEN0_GUI_LAYOUT_ID, boxes, layout::total_num_boxes(layout));
}

void gui::evaluate_screen0_gui(const Screen0GUIContext& context) {
  elements::evaluate(&globals.data.elements, &context.cursor_state, (void*) &context);
  elements::end_elements(&globals.data.elements);
}

void gui::render_screen0_gui(const Screen0GUIContext& context) {
  auto& draw_list = globals.data.draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, globals.data.layout);
  gui::push_draw_list(context.render_data, draw_list);
}

void gui::terminate_screen0_gui() {
  gui::layout::destroy_layout(&globals.data.layout);
}

GROVE_NAMESPACE_END
