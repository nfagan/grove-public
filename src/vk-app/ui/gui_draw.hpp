#pragma once

#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_elements.hpp"
#include "../render/render_gui_data.hpp"
#include "../render/font.hpp"

namespace grove::gui {

struct DrawableBox {
public:
  struct Flags {
    static constexpr uint8_t ManuallyPositioned = uint8_t(1);
    static constexpr uint8_t SmallUnlessHovered = uint8_t(2);
  };

public:
  bool is_small_unless_hovered() const {
    return flags & Flags::SmallUnlessHovered;
  }
  void set_small_unless_hovered() {
    flags |= Flags::SmallUnlessHovered;
  }

  bool is_manually_positioned() const {
    return flags & Flags::ManuallyPositioned;
  }
  void set_manually_positioned() {
    flags |= Flags::ManuallyPositioned;
  }

public:
  int layer{};
  layout::BoxID box_id{};
  gui::RenderQuadDescriptor quad_desc{};
  uint8_t flags{};
};

struct BoxDrawList {
  void clear() {
    drawables.clear();
  }
  std::vector<DrawableBox> drawables;
};

void draw_label(
  gui::RenderData* render_data, const gui::layout::ReadBox& label_box, const char* label,
  font::FontHandle font, float font_size, const Vec3f& color, float xpad, bool center_x);

void draw_dropdown_labels(
  gui::RenderData* render_data, const layout::Layout* layout, int box_begin, int box_end,
  const elements::DropdownData* dropdown_data, font::FontHandle font, const char** options,
  float font_size, const Vec3f& color);

DrawableBox* draw_box(
  BoxDrawList& draw_list, const layout::Layout* layout, int box,
  const gui::RenderQuadDescriptor& desc, int layer = 0);

void draw_boxes(BoxDrawList& draw_list, const layout::Layout* layout, int box_begin, int box_end,
                const gui::RenderQuadDescriptor& desc, int layer);

void draw_slider_boxes(
  BoxDrawList& draw_list, const layout::Layout* layout,
  int slider_section, int handle, const gui::RenderQuadDescriptor& slider_style,
  const gui::RenderQuadDescriptor& handle_style, int layer = 0);

void modify_style_from_cursor_events(
  BoxDrawList& draw_list, const cursor::CursorState* cursor, float hover_color_scale);
void set_box_quad_positions(BoxDrawList& draw_list, const layout::Layout* layout);
void modify_box_quad_positions_from_cursor_events(
  BoxDrawList& draw_list, const cursor::CursorState* cursor, float small_scale);
void push_draw_list(gui::RenderData* render_data, const BoxDrawList& draw_list);

}