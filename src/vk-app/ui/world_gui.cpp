#include "world_gui.hpp"
#include "ui_common.hpp"
#include "tree_gui.hpp"
#include "roots_gui.hpp"
#include "arch_gui.hpp"
#include "flower_gui.hpp"
#include "../render/font.hpp"
#include "../render/render_gui_data.hpp"
#include "grove/gui/font.hpp"
#include "grove/gui/gui_elements.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/input/KeyTrigger.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include <vector>

#define BOXIDI(i) grove::gui::layout::BoxID::create(GROVE_WORLD_GUI_LAYOUT_ID, (i))

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

struct WorldGUIData;
struct PendingBox;
using ClickCallback = void(PendingBox&, WorldGUIData&, const WorldGUIContext&);

enum class TabMode {
  Trees,
  Roots,
  Flower,
  Structure
};

struct PendingBox {
  Optional<gui::RenderQuadDescriptor> quad_desc;
  layout::BoxID box_id{};
  TabMode to_tab_mode{};
  ClickCallback* left_click_callback{};
};

struct WorldGUIData {
  layout::Layout* layout{};
  std::vector<PendingBox> pending;
  elements::Elements elements;
  TabMode mode{};
  Stopwatch stopwatch;
};

void change_tab_mode(PendingBox& box, WorldGUIData& data, const WorldGUIContext&) {
  data.mode = box.to_tab_mode;
}

void prepare_world_gui(WorldGUIData& data, const WorldGUIContext& context) {
  if (!data.layout) {
    data.layout = layout::create_layout(GROVE_WORLD_GUI_LAYOUT_ID);
  }

  auto* layout = data.layout;
  layout::clear_layout(layout);
  data.pending.clear();
  elements::begin_elements(&data.elements, GROVE_WORLD_GUI_LAYOUT_ID);

  clear_tree_gui();
  clear_roots_gui();
  clear_flower_gui();
  clear_arch_gui();

  if (context.hidden) {
    return;
  }

  auto maybe_text_font = font::get_text_font();
  if (!maybe_text_font) {
    return;
  }

  const auto text_font = maybe_text_font.value();

  auto fb_dims = context.container_dimensions;
  layout::set_root_dimensions(layout, fb_dims.x, fb_dims.y);

//  double t_rate = std::sin(data.stopwatch.delta().count() * 0.5);
//  const float root_w = 512.0f + float(t_rate * 0.0f) * 256.0f;
  const float root_w = 512.0f;
  const float root_h = 600.0f;

  layout::begin_group(layout, 0, layout::GroupOrientation::Col);
  int root = layout::box(layout, {1, root_w, root_w}, {1, root_h, root_h});
  layout::end_group(layout);

  layout::begin_group(layout, root, layout::GroupOrientation::Row);
  const int tab_head = layout::box(layout, {1.0f}, {0.25f});
  const int body = layout::box(layout, {1.0f}, {0.75f});
  layout::end_group(layout);

  const float font_size = ui::Constants::font_size;

  {
    constexpr int num_modes = 4;
    const char* texts[num_modes] = {"trees", "roots", "flowers", "structure"};
    float tab_ws[num_modes];
    const float tab_h = 32.0f;
    for (int i = 0; i < num_modes; i++) {
      tab_ws[i] = ui::font_sequence_width_ascii(text_font, texts[i], font_size, 8.0f);
    }

    int tabs[num_modes];
    layout::begin_group(layout, tab_head, layout::GroupOrientation::Col);
    for (int i = 0; i < num_modes; i++) {
      tabs[i] = layout::box(layout, {1, tab_ws[i], tab_ws[i]}, {1, tab_h, tab_h});
      layout::set_box_cursor_events(layout, tabs[i], {layout::BoxCursorEvents::Click});
    }
    layout::end_group(layout);

    const TabMode tab_modes[num_modes] = {
      TabMode::Trees, TabMode::Roots, TabMode::Flower, TabMode::Structure};

    for (int i = 0; i < num_modes; i++) {
      auto& pend = data.pending.emplace_back();
      pend.quad_desc = ui::make_render_quad_desc(layout::read_box(layout, tabs[i]), Vec3f{1.0f});
      pend.quad_desc.value().border_px = 2.0f;
      if (data.mode == tab_modes[i]) {
        pend.quad_desc.value().linear_border_color = Vec3f{1.0f, 0.0f, 0.0f};
      }
      pend.to_tab_mode = tab_modes[i];
      pend.box_id = BOXIDI(tabs[i]);
      pend.left_click_callback = change_tab_mode;
    }

    font::FontBitmapSampleInfo sample_infos[128];
    for (int i = 0; i < num_modes; i++) {
      int num_gen = ui::make_font_bitmap_sample_info_ascii(
        layout::read_box(layout, tabs[i]), text_font, texts[i], font_size, sample_infos, Vec2<bool>{true});
      gui::draw_glyphs(context.render_data, sample_infos, num_gen, Vec3f{});
    }
  }

  layout::begin_group(layout, body, layout::GroupOrientation::Row);
  const int row0 = layout::box(layout, {1}, {1});
  layout::end_group(layout);

  switch (data.mode) {
    case TabMode::Trees: {
      prepare_tree_gui(layout, row0, data.elements, context);
      break;
    }
    case TabMode::Roots: {
      prepare_roots_gui(layout, row0, data.elements, context);
      break;
    }
    case TabMode::Flower: {
      prepare_flower_gui(layout, row0, data.elements, context);
      break;
    }
    case TabMode::Structure: {
      prepare_arch_gui(layout, row0, data.elements, context);
      break;
    }
    default: {
      assert(false);
    }
  }

  auto* cursor = &context.cursor_state;
  const auto* boxes = layout::read_box_slot_begin(layout);
  cursor::evaluate_boxes(cursor, GROVE_WORLD_GUI_LAYOUT_ID, boxes, layout::total_num_boxes(layout));
}

void evaluate_world_gui(WorldGUIData& data, const WorldGUIContext& context) {
  auto* cursor_state = &context.cursor_state;

  for (auto& pend : data.pending) {
    if (pend.left_click_callback && cursor::left_clicked_on(cursor_state, pend.box_id)) {
      pend.left_click_callback(pend, data, context);
    }
  }

  elements::evaluate(&data.elements, cursor_state, (void*) &context);
  elements::end_elements(&data.elements);

  if (!context.hidden && context.key_trigger.newly_pressed(Key::GraveAccent)) {
    if (context.key_trigger.is_pressed(Key::LeftShift)) {
      data.mode = int(data.mode) == 0 ? TabMode(3) : TabMode(int(data.mode) - 1);
    } else {
      data.mode = TabMode((int(data.mode) + 1) % 4);
    }
  }
}

void render_world_gui(WorldGUIData& data, const WorldGUIContext& context) {
  auto* cursor_state = &context.cursor_state;
  for (auto& pend : data.pending) {
    if (pend.quad_desc) {
      if (cursor::hovered_over(cursor_state, pend.box_id)) {
        pend.quad_desc.value().linear_color *= 0.75f;
      }
      gui::draw_quads(context.render_data, &pend.quad_desc.value(), 1);
    }
  }

  render_tree_gui(data.layout, context);
  render_roots_gui(data.layout, context);
  render_flower_gui(data.layout, context);
  render_arch_gui(data.layout, context);
}

void terminate(WorldGUIData& data) {
  layout::destroy_layout(&data.layout);
}

struct {
  WorldGUIData data;
} globals;

} //  anon

void gui::prepare_world_gui(const WorldGUIContext& context) {
  prepare_world_gui(globals.data, context);
}

void gui::evaluate_world_gui(const WorldGUIContext& context) {
  evaluate_world_gui(globals.data, context);
}

void gui::render_world_gui(const WorldGUIContext& context) {
  render_world_gui(globals.data, context);
}

void gui::terminate_world_gui() {
  terminate(globals.data);
}

#undef BOXIDI

GROVE_NAMESPACE_END
