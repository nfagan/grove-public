#pragma once

#include "grove/math/vector.hpp"

namespace grove::gui {
struct RenderQuadDescriptor;
}

namespace grove::gui::layout {
struct ReadBox;
}

namespace grove::font {
struct FontHandle;
struct FontBitmapSampleInfo;
}

namespace grove::ui {

struct Constants {
  static constexpr float font_size = 24.0f;
  static constexpr float line_height = 32.0f;
};

gui::RenderQuadDescriptor
make_render_quad_desc(const gui::layout::ReadBox& box, const Vec3f& color,
                      float border = 0.0f, const Vec3f& border_color = {},
                      float radius_frac = 0.0f, float trans = 0.0f);

gui::RenderQuadDescriptor
make_render_quad_desc_style(const Vec3f& color,
                            float border = 0.0f, const Vec3f& border_color = {},
                            float radius_frac = 0.0f, float trans = 0.0f);

void set_render_quad_desc_positions(gui::RenderQuadDescriptor& desc, const gui::layout::ReadBox& box);

int make_font_bitmap_sample_info_ascii(
  const gui::layout::ReadBox& box, const font::FontHandle& font,
  const char* text, float font_size, font::FontBitmapSampleInfo* sample_infos,
  const Vec2<bool>& center, float* xoff = nullptr, float* yoff = nullptr);

float font_sequence_width_ascii(const font::FontHandle& font, const char* text, float font_size,
                                float pad_lr = 0.0f, bool ceil_to_int = true);

}