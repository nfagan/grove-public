#include "gui_draw.hpp"
#include "ui_common.hpp"
#include "grove/gui/font.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/math/util.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void gui::draw_label(
  gui::RenderData* render_data, const gui::layout::ReadBox& label_box, const char* label,
  font::FontHandle font, float font_size, const Vec3f& color, float xpad, bool center_x) {
  //
  Temporary<font::FontBitmapSampleInfo, 256> store_sample_infos;
  auto* sample_infos = store_sample_infos.require(int(std::strlen(label)));
  int num_gen = ui::make_font_bitmap_sample_info_ascii(
    label_box, font, label, font_size, sample_infos, {center_x, true}, &xpad);
  gui::draw_glyphs(render_data, sample_infos, num_gen, color);
}

void gui::draw_dropdown_labels(
  gui::RenderData* render_data, const layout::Layout* layout, int box_begin, int box_end,
  const elements::DropdownData* dropdown_data, font::FontHandle font, const char** options,
  float font_size, const Vec3f& color) {
  //
  const int dst_layer = dropdown_data->open ? 1 : 0;
  for (int i = box_begin; i < box_end; i++) {
    const int opt = dropdown_data->open ? (i - box_begin) : dropdown_data->option;
    const char* opt_text = options[opt];

    Temporary<font::FontBitmapSampleInfo, 256> store_sample_infos;
    auto* sample_infos = store_sample_infos.require(int(std::strlen(opt_text)));

    auto box = layout::read_box(layout, i);
    int num_gen = ui::make_font_bitmap_sample_info_ascii(
      box, font, opt_text, font_size, sample_infos, Vec2<bool>{true});
    gui::draw_glyphs(render_data, sample_infos, num_gen, color, dst_layer);
  }
}

gui::DrawableBox* gui::draw_box(
  BoxDrawList& draw_list, const layout::Layout* layout, int box,
  const gui::RenderQuadDescriptor& desc, int layer) {
  //
  if (!layout::is_fully_clipped_box(layout, box)) {
    auto& pend = draw_list.drawables.emplace_back();
    pend.box_id = layout::BoxID::create(layout::get_id(layout), box);
    pend.quad_desc = desc;
    pend.layer = layer;
    return &pend;
  } else {
    return nullptr;
  }
}

void gui::draw_boxes(
  BoxDrawList& draw_list, const layout::Layout* layout, int box_begin, int box_end,
  const gui::RenderQuadDescriptor& desc, int layer) {
  //
  for (int i = box_begin; i < box_end; i++) {
    draw_box(draw_list, layout, i, desc, layer);
  }
}

void gui::draw_slider_boxes(
  BoxDrawList& draw_list, const layout::Layout* layout,
  int slider_section, int handle, const gui::RenderQuadDescriptor& slider_style,
  const gui::RenderQuadDescriptor& handle_style, int layer) {
  //
  draw_box(draw_list, layout, slider_section, slider_style, layer);
  draw_box(draw_list, layout, handle, handle_style, layer);
}

void gui::push_draw_list(gui::RenderData* render_data, const BoxDrawList& draw_list) {
  for (auto& drawable : draw_list.drawables) {
    gui::draw_quads(render_data, &drawable.quad_desc, 1, drawable.layer);
  }
}

void gui::set_box_quad_positions(BoxDrawList& draw_list, const layout::Layout* layout) {
  for (auto& drawable : draw_list.drawables) {
    if (!drawable.is_manually_positioned()) {
      auto box = layout::read_box(layout, drawable.box_id.index());
      ui::set_render_quad_desc_positions(drawable.quad_desc, box);
    }
  }
}

void gui::modify_box_quad_positions_from_cursor_events(
  BoxDrawList& draw_list, const cursor::CursorState* cursor, float small_scale) {
  //
  for (auto& drawable : draw_list.drawables) {
    if (drawable.is_small_unless_hovered() && !cursor::hovered_over(cursor, drawable.box_id)) {
      Vec2f cent = lerp(0.5f, drawable.quad_desc.true_p0, drawable.quad_desc.true_p1);
      auto new_size = (drawable.quad_desc.true_p1 - drawable.quad_desc.true_p0) * small_scale;
      drawable.quad_desc.true_p0 = cent - new_size * 0.5f;
      drawable.quad_desc.true_p1 = cent + new_size * 0.5f;
      drawable.quad_desc.clip_p0 = max(drawable.quad_desc.clip_p0, drawable.quad_desc.true_p0);
      drawable.quad_desc.clip_p1 = min(drawable.quad_desc.clip_p1, drawable.quad_desc.true_p1);
    }
  }
}

void gui::modify_style_from_cursor_events(
  BoxDrawList& draw_list, const cursor::CursorState* cursor, float hover_color_scale) {
  //
  for (auto& drawable : draw_list.drawables) {
    if (cursor::hovered_over(cursor, drawable.box_id)) {
      drawable.quad_desc.linear_color *= hover_color_scale;
    }
  }
}

GROVE_NAMESPACE_END
