#pragma once

#include "render_gui_types.hpp"
#include <vector>

namespace grove::font {
struct FontBitmapSampleInfo;
}

namespace grove::gui {

struct RenderQuadDescriptor {
  Vec2f clip_p0;
  Vec2f clip_p1;
  Vec2f true_p0;
  Vec2f true_p1;
  Vec3f linear_color;
  Vec3f linear_border_color;
  float border_px; //  px
  float radius_fraction; //  [0, 1]
  float translucency;
};

struct RenderData {
  static constexpr int max_num_gui_layers = 2;

  std::vector<GlyphQuadVertex> glyph_vertices[max_num_gui_layers];
  std::vector<uint16_t> glyph_vertex_indices[max_num_gui_layers];

  std::vector<QuadVertex> quad_vertices[max_num_gui_layers];
  std::vector<uint16_t> quad_vertex_indices[max_num_gui_layers];

  uint32_t max_glyph_image_index{};
};

RenderData* get_global_gui_render_data();
void begin_update(RenderData* data);
void draw_quads(RenderData* data, const RenderQuadDescriptor* descs, int num_descs, int layer = 0);
void draw_glyphs(RenderData* data, const font::FontBitmapSampleInfo* samples, int num_samples,
                 const Vec3f& linear_color, int layer = 0);

}