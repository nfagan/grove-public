#include "menu_gui.hpp"
#include "help_gui.hpp"
#include "audio_settings_gui.hpp"
#include "graphics_settings_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "grove/common/common.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"

GROVE_NAMESPACE_BEGIN

using namespace gui;

namespace {

enum class TabMode {
  Help = 0,
  AudioSettings,
  GraphicsSettings
};

struct MenuGUIData {
  layout::Layout* layout{};
  elements::Elements elements;
  BoxDrawList draw_list;
  TabMode mode{TabMode::Help};
};

void set_mode_audio_settings(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  auto* data = static_cast<MenuGUIData*>(ctx->gui_data);
  data->mode = TabMode::AudioSettings;
}

void set_mode_graphics_settings(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  auto* data = static_cast<MenuGUIData*>(ctx->gui_data);
  data->mode = TabMode::GraphicsSettings;
}

void set_mode_help(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  auto* data = static_cast<MenuGUIData*>(ctx->gui_data);
  data->mode = TabMode::Help;
}

void enable_tutorial(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  ctx->gui_result->enable_tutorial_gui = true;
}

void close_gui(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  ctx->gui_result->close_gui = true;
}

void quit_app(void* context) {
  auto* ctx = static_cast<const MenuGUIContext*>(context);
  ctx->gui_result->quit_app = true;
}

void prepare_menu_gui(MenuGUIData& data, const MenuGUIContext& context) {
  if (!data.layout) {
    data.layout = gui::layout::create_layout(GROVE_MENU_GUI_LAYOUT_ID);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.draw_list.clear();
  elements::begin_elements(&data.elements, GROVE_MENU_GUI_LAYOUT_ID);

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

  layout::begin_group(layout, 0, layout::GroupOrientation::Col);
  int root = layout::box(layout, {1}, {1});
  layout::end_group(layout);

//  draw_box(data.draw_list, layout, root, {});

  layout::begin_group(layout, root, layout::GroupOrientation::Col);
  const int container = layout::box(layout, {1, 768, 768}, {1, 512, 512});
  layout::end_group(layout);

  draw_box(data.draw_list, layout, container, ui::make_render_quad_desc_style(Vec3f{0.5f}, {}, {}, {}, 0.25f));

  layout::begin_group(layout, container, layout::GroupOrientation::Col);
  int col0 = layout::box(layout, {0.25f}, {1});
  int col1 = layout::box(layout, {0.75f}, {1});
  layout::end_group(layout);

  layout::begin_group(layout, col0, layout::GroupOrientation::Row);
  int options_box = layout::box(layout, {0.5f}, {0.75f});
  layout::end_group(layout);

  layout::begin_group(layout, options_box, layout::GroupOrientation::Row, 0, 0, layout::JustifyContent::Left);
  int row0 = prepare_row(layout, line_h, 0);
  int row1 = prepare_row(layout, line_h, line_space);
  int row2 = prepare_row(layout, line_h, line_space);
  int row3 = prepare_row(layout, line_h, line_space);
  int row4 = prepare_row(layout, line_h, line_space);
  int row5 = prepare_row(layout, line_h, line_space);
  layout::end_group(layout);

  auto button_row = [&](int row, const char* text, bool highlighted, elements::ClickCallback* cb) {
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    layout::begin_group(layout, row, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
    int button = prepare_button(data.elements, layout, {1, bw, bw}, line_h, false, cb);
    layout::end_group(layout);

    const auto border_color = highlighted ? Vec3f{1.0f, 0.0f, 0.0f} : Vec3f{1.0f};
    draw_label(context.render_data, layout::read_box(layout, button), text, text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.draw_list, layout, button, ui::make_render_quad_desc_style(Vec3f{1.0f}, 2.0f, border_color));
  };

  button_row(row0, "close", false, close_gui);
  button_row(row1, "help", data.mode == TabMode::Help, set_mode_help);
  button_row(row2, "tutorial", false, enable_tutorial);
  button_row(row3, "audio", data.mode == TabMode::AudioSettings, set_mode_audio_settings);
  button_row(row4, "graphics", data.mode == TabMode::GraphicsSettings, set_mode_graphics_settings);
  button_row(row5, "quit", false, quit_app);

  if (data.mode == TabMode::Help) {
    prepare_help_gui(layout, col1, data.elements, data.draw_list, context);
  } else if (data.mode == TabMode::AudioSettings) {
    prepare_audio_settings_gui(layout, col1, data.elements, data.draw_list, context);
  } else if (data.mode == TabMode::GraphicsSettings) {
    prepare_graphics_settings_gui(layout, col1, data.elements, data.draw_list, context);
  }

  auto* cursor = &context.cursor_state;
  const auto* boxes = layout::read_box_slot_begin(layout);
  cursor::evaluate_boxes(cursor, GROVE_MENU_GUI_LAYOUT_ID, boxes, layout::total_num_boxes(layout));
}

void evaluate_menu_gui(MenuGUIData& data, const MenuGUIContext& context) {
  elements::evaluate(&data.elements, &context.cursor_state, (void*) &context);
  elements::end_elements(&data.elements);
}

void render_menu_gui(MenuGUIData& data, const MenuGUIContext& context) {
  auto& draw_list = data.draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, data.layout);
  gui::push_draw_list(context.render_data, draw_list);
}

void terminate_menu_gui(MenuGUIData& data) {
  layout::destroy_layout(&data.layout);
}

struct {
  MenuGUIData data;
} globals;

} //  anon

void* gui::get_global_menu_gui_data() {
  return &globals.data;
}

void gui::prepare_menu_gui(const MenuGUIContext& context) {
  grove::prepare_menu_gui(globals.data, context);
}

void gui::evaluate_menu_gui(const MenuGUIContext& context) {
  grove::evaluate_menu_gui(globals.data, context);
}

void gui::render_menu_gui(const MenuGUIContext& context) {
  grove::render_menu_gui(globals.data, context);
}

void gui::terminate_menu_gui() {
  grove::terminate_menu_gui(globals.data);
}

GROVE_NAMESPACE_END
