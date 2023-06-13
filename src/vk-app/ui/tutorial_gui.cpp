#include "tutorial_gui.hpp"
#include "ui_common.hpp"
#include "gui_draw.hpp"
#include "gui_components.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/gui/font.hpp"
#include "grove/visual/image_process.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct TutorialGUIData {
  static constexpr int num_slides = 9;

  gui::BoxDrawList draw_list;
  gui::elements::Elements elements;
  gui::layout::Layout* layout{};
  int slide_index{};
};

struct {
  TutorialGUIData data;
} globals;

#if 0
void close_gui(void* context) {
  auto* ctx = static_cast<const TutorialGUIContext*>(context);
  ctx->gui_result->close_screen = true;
}
#endif

void next_slide(void*) {
  int si = std::min(globals.data.slide_index + 1, TutorialGUIData::num_slides - 1);
  globals.data.slide_index = si;
}

void prev_slide(void*) {
  int si = std::max(0, globals.data.slide_index - 1);
  globals.data.slide_index = si;
}

void quit_tutorial(void* context) {
  auto* ctx = static_cast<TutorialGUIContext*>(context);
  ctx->gui_result->close_screen = true;
}

} //  anon

void gui::jump_to_first_tutorial_gui_slide() {
  globals.data.slide_index = 0;
}

void gui::prepare_tutorial_gui(const TutorialGUIContext& context) {
  auto& data = globals.data;

  if (!data.layout) {
    data.layout = gui::layout::create_layout(GROVE_TUTORIAL_GUI_LAYOUT_ID);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.draw_list.clear();
  elements::begin_elements(&data.elements, GROVE_TUTORIAL_GUI_LAYOUT_ID);

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

  const float cont_h = 640.0f;
  layout::begin_group(layout, root, layout::GroupOrientation::Col);
  const int cont0 = layout::box(layout, {0.25f}, {1, cont_h, cont_h});
  const int cont1 = layout::box(layout, {0.25f}, {1, cont_h, cont_h});
  const int cont2 = layout::box(layout, {0.25f}, {1, cont_h, cont_h});
  const int cont3 = layout::box(layout, {0.25f}, {1, cont_h, cont_h});
  layout::end_group(layout);

  (void) cont1;
  (void) cont2;
  (void) cont3;

  layout::begin_group(layout, cont0, layout::GroupOrientation::Row);
  const int text_portion = layout::box(layout, {1}, {0.75f});
  const int button_portion = layout::box(layout, {1}, {0.25f});
  layout::end_group(layout);

  const char* txts[TutorialGUIData::num_slides]{
    "Welcome to grove! This is the beta version of this program; many features are incomplete and / or buggy. Nevertheless, I hope that it is still worth exploring in its current state. You can access this tutorial at any time via the main menu (press escape to open it).",
    "To start, try interacting with the GUI. To show or hide the GUI, press alt + f on the keyboard. When the GUI is open, pressing tab toggles between modes, and ` (the key above tab) toggles between sub-modes.",
    "Try pressing tab and/or ` to navigate to the screen with a top row of five colored buttons. Click the green button in the top-right to create an output audio node. Then press ` on the keyboard, and click the transparent white button near the top-left to create a new MIDI output track.",
    "Each orange square is a MIDI output port. Other kinds of output ports also exist. When you see a white border surrounding a colored square, it means the port is an input port. Try clicking one of the orange output ports to select it; then, holding the left-control key, click on an orange input port in the world to connect these ports together. Try completing the circuit by connecting any remaining dark gray output ports into the input ports of the green node you created earlier. Also, if a port is connected to something, you can right click it to disconnect it.",
    "Look around for the red \"cursor\" in the world. There are also blue and multi-colored ones. The red cursor indicates where new trees, roots, and flowers will spawn. Try moving the red cursor somewhere, then open the GUI (alt + f) and navigate to the tree menu. Press the create button to generate a new tree at this position.",
    "The blue cursor is the position to which roots are attracted. Try creating new roots, then move the blue cursor to change the resulting root forms.",
    "The multi-colored cursor controls the position of the current structure segment, and is more self-explanatory.",
    "Lastly, you can toggle between below-ground, above-ground, and on-ground views. Press alt + 1 for an overhead view (and alt + 1 again to return to ground), or alt + 2 to go below ground.",
    "That is all for now. Try adding more entities to the world and connecting them together."
  };

  const char* txt = txts[data.slide_index];
  Temporary<grove::font::FontBitmapSampleInfo, 2048> store_sample_infos;
  auto* sample_infos = store_sample_infos.require(2048);

  float x{};
  float y{};
  int num_gen = ui::make_font_bitmap_sample_info_ascii(
    layout::read_box(layout, text_portion),
    text_font, txt, font_size, sample_infos, Vec2<bool>{}, &x, &y);

  gui::draw_glyphs(context.render_data, sample_infos, num_gen, Vec3f{});

  auto button_row = [&](int row, const char* text, bool highlighted, elements::ClickCallback* cb) {
    const float bw = ui::font_sequence_width_ascii(text_font, text, font_size, 4.0f);
    layout::begin_group(layout, row, layout::GroupOrientation::Col, 0, 0);
    int button = prepare_button(data.elements, layout, {1, bw, bw}, line_h, true, cb);
    layout::end_group(layout);

    const auto border_color = highlighted ? Vec3f{1.0f, 0.0f, 0.0f} : Vec3f{1.0f};
    const auto box_color = Vec3f{1.0f};
    draw_label(context.render_data, layout::read_box(layout, button), text, text_font, font_size, Vec3f{}, 0, true);
    draw_box(data.draw_list, layout, button, ui::make_render_quad_desc_style(box_color, 0.0f, border_color));
  };

  layout::begin_group(layout, button_portion, layout::GroupOrientation::Col);
  const int prev_button = layout::box(layout, {0.33f}, line_h);
  const int next_button = layout::box(layout, {0.33f}, line_h);
  const int quit_button = layout::box(layout, {0.33f}, line_h);
  layout::end_group(layout);

  if (data.slide_index > 0) {
    button_row(prev_button, "previous", false, prev_slide);
  }
  if (data.slide_index + 1 < TutorialGUIData::num_slides) {
    button_row(next_button, "next", false, next_slide);
  }
  button_row(quit_button, "close", false, quit_tutorial);

  auto* cursor = &context.cursor_state;
  const auto* boxes = layout::read_box_slot_begin(layout);
  cursor::evaluate_boxes(cursor, GROVE_TUTORIAL_GUI_LAYOUT_ID, boxes, layout::total_num_boxes(layout));
}

void gui::evaluate_tutorial_gui(const TutorialGUIContext& context) {
  elements::evaluate(&globals.data.elements, &context.cursor_state, (void*) &context);
  elements::end_elements(&globals.data.elements);
}

void gui::render_tutorial_gui(const TutorialGUIContext& context) {
  auto& draw_list = globals.data.draw_list;
  gui::modify_style_from_cursor_events(draw_list, &context.cursor_state, 0.75f);
  gui::set_box_quad_positions(draw_list, globals.data.layout);
  gui::push_draw_list(context.render_data, draw_list);
}

void gui::terminate_tutorial_gui() {
  gui::layout::destroy_layout(&globals.data.layout);
}

GROVE_NAMESPACE_END
