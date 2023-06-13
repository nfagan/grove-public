#include "render_gui_data.hpp"
#include "grove/gui/font.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"
#include "grove/math/util.hpp"
#include "grove/visual/geometry.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

uint32_t pack_color(Vec3f c) {
  c = clamp_each(c, Vec3f{}, Vec3f{1.0f}) * 255.0f;
  return pack::pack_4u8_1u32(uint8_t(c.x), uint8_t(c.y), uint8_t(c.z), 0);
}

void gen_quad_vertices(const RenderQuadDescriptor* descs, int num_descs,
                       std::vector<QuadVertex>& vertices,
                       std::vector<uint16_t>& vertex_indices) {
  uint16_t quad_indices[6];
  geometry::get_quad_indices(quad_indices);

  for (int i = 0; i < num_descs; i++) {
    auto& desc = descs[i];

    Vec2f dims = desc.true_p1 - desc.true_p0;
    Vec2f cent = dims * 0.5f + desc.true_p0;
    Vec4f instance_centroid_and_dimensions{cent.x, cent.y, dims.x, dims.y};

    QuadVertex qverts[4];
    qverts[0].xy_unused = Vec4f{desc.clip_p0.x, desc.clip_p0.y, 0.0f, 0.0f};
    qverts[1].xy_unused = Vec4f{desc.clip_p1.x, desc.clip_p0.y, 0.0f, 0.0f};
    qverts[2].xy_unused = Vec4f{desc.clip_p1.x, desc.clip_p1.y, 0.0f, 0.0f};
    qverts[3].xy_unused = Vec4f{desc.clip_p0.x, desc.clip_p1.y, 0.0f, 0.0f};

    const float opacity = clamp01(1.0f - desc.translucency);
    for (auto& qv : qverts) {
      qv.instance_centroid_and_dimensions = instance_centroid_and_dimensions;
      qv.instance_radius_fraction_and_border_size_and_opacity = Vec4f{
        desc.radius_fraction, desc.border_px, opacity, 0.0f};
      qv.instance_color_and_border_color = Vec4<uint32_t>{
        pack_color(desc.linear_color),
        pack_color(desc.linear_border_color),
        0, 0
      };
    }

    for (uint16_t ind : quad_indices) {
      vertex_indices.push_back(ind + uint16_t(vertices.size()));
    }

    vertices.insert(vertices.end(), qverts, qverts + 4);
  }
}

struct {
  RenderData data;
} globals;

} //  anon

RenderData* gui::get_global_gui_render_data() {
  return &globals.data;
}

void gui::begin_update(RenderData* data) {
  for (int i = 0; i < RenderData::max_num_gui_layers; i++) {
    data->quad_vertices[i].clear();
    data->quad_vertex_indices[i].clear();
    data->glyph_vertices[i].clear();
    data->glyph_vertex_indices[i].clear();
  }
}

void gui::draw_quads(RenderData* data, const RenderQuadDescriptor* descs, int num_descs, int layer) {
  gen_quad_vertices(descs, num_descs, data->quad_vertices[layer], data->quad_vertex_indices[layer]);
}

void gui::draw_glyphs(RenderData* data, const font::FontBitmapSampleInfo* samples, int num_samples,
                      const Vec3f& linear_color, int layer) {
  auto& dst_verts = data->glyph_vertices[layer];
  auto& dst_inds = data->glyph_vertex_indices[layer];

  uint16_t quad_indices[6];
  geometry::get_quad_indices(quad_indices);

  const uint32_t col = pack_color(linear_color);
  for (int i = 0; i < num_samples; i++) {
    auto& s = samples[i];
    assert(s.bitmap_index >= 0);

    const auto bm_index = uint32_t(s.bitmap_index);
    data->max_glyph_image_index = std::max(data->max_glyph_image_index, bm_index);

    GlyphQuadVertex vs[4];
    vs[0] = {Vec4f{s.x0, s.y0, s.u0, s.v0}, Vec4<uint32_t>{bm_index, col, 0, 0}};
    vs[1] = {Vec4f{s.x1, s.y0, s.u1, s.v0}, Vec4<uint32_t>{bm_index, col, 0, 0}};
    vs[2] = {Vec4f{s.x1, s.y1, s.u1, s.v1}, Vec4<uint32_t>{bm_index, col, 0, 0}};
    vs[3] = {Vec4f{s.x0, s.y1, s.u0, s.v1}, Vec4<uint32_t>{bm_index, col, 0, 0}};

    for (uint16_t ind : quad_indices) {
      dst_inds.push_back(ind + uint16_t(dst_verts.size()));
    }

    dst_verts.insert(dst_verts.end(), vs, vs + 4);
  }
}

GROVE_NAMESPACE_END
