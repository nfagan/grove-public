#include "render_resource_flow_along_nodes_particles.hpp"
#include "render_particles_gpu.hpp"
#include "../procedural_tree/resource_flow_along_nodes.hpp"
#include "grove/math/util.hpp"
#include "grove/visual/geometry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

struct Config {
  static constexpr int lod0_segments = 16;
  static constexpr int lod1_segments = 8;
  static constexpr int lod2_segments = 4;
  static constexpr float lod0_dist = 16.0f;
  static constexpr float lod1_dist = 64.0f;
};

Vec3f apply_tform(float px, const SpiralAroundNodesQuadVertexTransform& tform, float s) {
  auto p0 = tform.frame_x * -s + tform.p;
  auto p1 = tform.frame_x * s + tform.p;
  return lerp(px, p0, p1);
};

void gen_spiral_around_nodes_quad_vertices(
  const SpiralAroundNodesUpdateContext* context, int num_segments,
  const float* src_verts, float* dst_verts, float scale,
  int num_ps, float i0f, float i1f) {
  //
  for (int i = 0; i < num_segments * 6; i++) {
    const float px = src_verts[i * 3] * 0.5f + 0.5f;
    const float py = src_verts[i * 3 + 1];

    float i0_base = std::max(lerp(py, i0f, i1f), 0.0f);
    float i0_t = i0_base - std::floor(i0_base);

    int i0 = clamp(int(i0_base), 0, num_ps - 1);
    int i1 = clamp(i0 + 1, 0, num_ps - 1);
    auto& tform0 = context->points[i0];
    auto& tform1 = context->points[i1];

    float s = scale * 0.125f * (1.0f - std::pow(std::abs(py * 2.0f - 1.0f), 2.0f));
    auto p = lerp(i0_t, apply_tform(px, tform0, s), apply_tform(px, tform1, s));

    for (int j = 0; j < 3; j++) {
      dst_verts[i * 3 + j] = p[j];
    }
  }
}

void gen_quad_vertices_burrowing(
  const SpiralAroundNodesUpdateContext* context, int num_segments,
  const float* src_verts, float* dst_verts, float scale) {
  //
  const int num_ps = context->point_segment1_end;
  const float eval_t = context->t;
  float i0f = float(context->point_segment0_end) +
    eval_t * float(context->point_segment1_end - context->point_segment0_end);
  auto i1f = float(context->point_segment1_end - 1);

  gen_spiral_around_nodes_quad_vertices(
    context, num_segments, src_verts, dst_verts, scale, num_ps, i0f, i1f);
}

void gen_quad_vertices(
  const SpiralAroundNodesUpdateContext* context, int num_segments,
  const float* src_verts, float* dst_verts, float scale) {
  //
  const int num_ps = context->point_segment1_end;
  const float eval_t = context->t;
  int seg1_size = num_ps - context->point_segment0_end;
  float i0f = float(context->point_segment0_end) * eval_t;
  float i1f = float(context->point_segment0_end - 1) + float(seg1_size) * eval_t;

  gen_spiral_around_nodes_quad_vertices(
    context, num_segments, src_verts, dst_verts, scale, num_ps, i0f, i1f);
}

int get_lod_quad_segments(float dist) {
  if (dist < Config::lod0_dist) {
    return Config::lod0_segments;
  } else if (dist < Config::lod1_dist) {
    return Config::lod1_segments;
  } else {
    return Config::lod2_segments;
  }
}

} //  anon

void particle::push_resource_flow_along_nodes_particles(
  const tree::SpiralAroundNodesUpdateContext* contexts, int num_contexts) {
  //
  for (int c = 0; c < num_contexts; c++) {
    auto& ctx = contexts[c];
    if (!ctx.active || ctx.point_segment0_end >= ctx.point_segment1_end) {
      continue;
    }

    constexpr int max_num_segments = 32;
    float src_verts[max_num_segments * 6 * 3];

    const int num_segments = std::min(max_num_segments, get_lod_quad_segments(ctx.distance_to_camera));
    geometry::get_segmented_quad_positions(num_segments, true, src_verts);

    const float scale_atten = ctx.fadeout ? 1.0f - ctx.fade_frac : ctx.fade_frac;
    const Vec3f color = Vec3f(ctx.color.x, ctx.color.y, ctx.color.z) / 255.0f;
    const float scale = ctx.scale * scale_atten;
    const float trans = ctx.render_pipeline_index == 0 ? 0.0f : 0.5f;

    float dst_verts[max_num_segments * 6 * 3];
    if (ctx.burrowing) {
      gen_quad_vertices_burrowing(&ctx, num_segments, src_verts, dst_verts, scale);
    } else {
      gen_quad_vertices(&ctx, num_segments, src_verts, dst_verts, scale);
    }

    particle::SegmentedQuadVertexDescriptor vert_descs[max_num_segments * 6];
    for (int i = 0; i < num_segments * 6; i++) {
      auto& desc = vert_descs[i];
      desc.position = Vec3f{dst_verts[i * 3], dst_verts[i * 3 + 1], dst_verts[i * 3 + 2]};
      desc.color = color;
      desc.translucency = trans;
      desc.min_depth_weight = 0;
    }

    if (ctx.render_pipeline_index == 0) {
      particle::push_segmented_quad_particle_vertices(vert_descs, num_segments * 6);
    } else {
      assert(ctx.render_pipeline_index == 1);
      particle::push_segmented_quad_sample_depth_image_particle_vertices(vert_descs, num_segments * 6);
    }
  }
}

GROVE_NAMESPACE_END