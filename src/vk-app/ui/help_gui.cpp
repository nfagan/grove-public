#include "help_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "grove/common/common.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void gui::prepare_help_gui(
  layout::Layout* layout, int box, elements::Elements& elements,
  BoxDrawList& draw_list, const MenuGUIContext& context) {
  //
  (void) elements;

  layout::begin_group(layout, box, layout::GroupOrientation::Row);
  int container = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  draw_box(draw_list, layout, container, ui::make_render_quad_desc_style(Vec3f{0.25f}, {}, {}, {}, 0.25f));

  layout::begin_group(layout, container, layout::GroupOrientation::Row);
  int sub_container = layout::box(layout, {0.75f}, {0.75f});
  layout::end_group(layout);

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();
  (void) text_font;

  const float font_size = ui::Constants::font_size;
  const float line_space = ui::Constants::line_height;
  const auto line_h = layout::BoxDimensions{1, line_space, line_space};

  layout::begin_group(layout, sub_container, layout::GroupOrientation::Row);
  int scrollable = layout::box(layout, {1}, {1});
  layout::set_box_cursor_events(layout, scrollable, {layout::BoxCursorEvents::Scroll});
  layout::end_group(layout);

  float scroll_y{};
  read_scroll_offsets(
    &context.cursor_state,
    layout::BoxID::create(GROVE_MENU_GUI_LAYOUT_ID, scrollable), nullptr, &scroll_y);
  scroll_y = std::floor(scroll_y);

  layout::begin_group(layout, scrollable, layout::GroupOrientation::Row, 0, scroll_y, layout::JustifyContent::Left);
  int rows[32];
  int ri{};
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, 0);
  rows[ri++] = prepare_row(layout, line_h, line_space);
  rows[ri++] = prepare_row(layout, line_h, 0);
  layout::end_group(layout);

  auto text_row = [&](int row, const char* text, const Vec3f& color = Vec3f{1.0f}) {
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    layout::begin_group(layout, row, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int box = layout::box(layout, {1, bw, bw}, line_h, false);
    layout::end_group(layout);
    draw_label(context.render_data, layout::read_box(layout, box), text, text_font, font_size, color, 0, true);
  };

  auto desc_color = Vec3f{0.75f};

  int dri{};
  text_row(rows[dri++], "w, a, s, d");
  text_row(rows[dri++], "moves the camera", desc_color);

  text_row(rows[dri++], "shift + mouse movement");
  text_row(rows[dri++], "rotates the camera", desc_color);

  text_row(rows[dri++], "alt + f");
  text_row(rows[dri++], "shows or hides the ui", desc_color);

  text_row(rows[dri++], "tab");
  text_row(rows[dri++], "cycles between ui modes", desc_color);

  text_row(rows[dri++], "`");
  text_row(rows[dri++], "cycles between panels within a ui mode", desc_color);

  text_row(rows[dri++], "ctrl + click");
  text_row(rows[dri++], "selects and connects ports", desc_color);

  text_row(rows[dri++], "right click");
  text_row(rows[dri++], "disconnects ports, if they are connected", desc_color);

  text_row(rows[dri++], "alt + click");
  text_row(rows[dri++], "isolates (solos) an input or output", desc_color);

  text_row(rows[dri++], "alt + x");
  text_row(rows[dri++], "toggles the music keyboard on or off", desc_color);
  text_row(rows[dri++], "(when on, movement is disabled)", desc_color);

  text_row(rows[dri++], "alt + 1 or 2");
  text_row(rows[dri++], "switches between camera views", desc_color);

  assert(dri == ri && "Wrong number of rows");
}

GROVE_NAMESPACE_END
