#include "ui_common.hpp"
#include "../render/render_gui_data.hpp"
#include "grove/gui/gui_layout.hpp"
#include "grove/gui/font.hpp"
#include "../render/font.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

gui::RenderQuadDescriptor
ui::make_render_quad_desc(const gui::layout::ReadBox& box, const Vec3f& color,
                          float border, const Vec3f& border_color, float radius_frac, float trans) {
  gui::RenderQuadDescriptor desc{};
  desc.true_p0 = Vec2f{box.x0, box.y0};
  desc.true_p1 = Vec2f{box.x1, box.y1};
  desc.clip_p0 = Vec2f{box.clip_x0, box.clip_y0};
  desc.clip_p1 = Vec2f{box.clip_x1, box.clip_y1};
  desc.linear_color = color;
  desc.linear_border_color = border_color;
  desc.border_px = border;
  desc.radius_fraction = radius_frac;
  desc.translucency = trans;
  return desc;
}

gui::RenderQuadDescriptor
ui::make_render_quad_desc_style(const Vec3f& color, float border,
                                const Vec3f& border_color, float radius_frac, float trans) {
  gui::RenderQuadDescriptor desc{};
  desc.linear_color = color;
  desc.linear_border_color = border_color;
  desc.border_px = border;
  desc.radius_fraction = radius_frac;
  desc.translucency = trans;
  return desc;
}

void ui::set_render_quad_desc_positions(gui::RenderQuadDescriptor& desc,
                                        const gui::layout::ReadBox& box) {
  desc.true_p0 = Vec2f{box.x0, box.y0};
  desc.true_p1 = Vec2f{box.x1, box.y1};
  desc.clip_p0 = Vec2f{box.clip_x0, box.clip_y0};
  desc.clip_p1 = Vec2f{box.clip_x1, box.clip_y1};
}

float ui::font_sequence_width_ascii(const font::FontHandle& font, const char* text, float font_size,
                                    float pad_lr, bool ceil_to_int) {
  float res = font::get_glyph_sequence_width_ascii(font, text, font_size);
  if (ceil_to_int) {
    res = std::ceil(res);
  }
  return res + pad_lr * 2.0f;
}

int ui::make_font_bitmap_sample_info_ascii(
  const gui::layout::ReadBox& box, const font::FontHandle& font,
  const char* text, float font_size, font::FontBitmapSampleInfo* sample_infos,
  const Vec2<bool>& center, float* xoff, float* yoff) {
  //
  float x = xoff ? *xoff : 0.0f;
  float y = yoff ? *yoff : 0.0f;
  int num_gen = font::ascii_left_justified(
    font, text, font_size, box.content_width(), sample_infos, &x, &y);
  font::offset_bitmap_samples(sample_infos, num_gen, box.content_x0, box.content_y0);

  float rx{};
  float ry{};
  if (center.x) {
    rx = std::max(0.0f, box.content_width() - x) * 0.5f;
  }
  if (center.y) {
    ry = std::max(0.0f, box.content_height() - font_size) * 0.5f;
  }
  if (center.x || center.y) {
    font::offset_bitmap_samples(sample_infos, num_gen, rx, ry);
  }

  num_gen = font::clip_bitmap_samples(
    sample_infos, num_gen, box.clip_x0, box.clip_y0, box.clip_x1, box.clip_y1);

  if (xoff) {
    *xoff = x;
  }
  if (yoff) {
    *yoff = y;
  }

  return num_gen;
}

GROVE_NAMESPACE_END
