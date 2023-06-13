#pragma once

#include "grove/math/vector.hpp"
#include <vector>
#include <unordered_set>

namespace grove::arch {

struct GrowingTriangle {
  uint32_t src_ti;
  uint32_t src_edge_pi0;
  uint32_t src_edge_pi1;
};

struct RenderTriangleGrowthData {
  const uint32_t* src_tris;
  uint32_t num_src_tris;
  const void* src_p;
  uint32_t src_stride;
  uint32_t src_offset;
  void* dst_p;
  uint32_t dst_stride;
  uint32_t dst_offset;
};

struct RenderTriangleGrowthContext {
  RenderTriangleGrowthData data;
  std::vector<GrowingTriangle> growing;
  std::unordered_set<uint32_t> visited_ti;
  std::vector<uint32_t> pending_ti;
  float f;
};

struct RecedingTriangleSet {
  std::vector<GrowingTriangle> receding;
  float f;
  float incr_scale;
};

struct RenderTriangleRecedeContext {
  RenderTriangleGrowthData data;
  std::vector<uint32_t> pending_pi;
  std::unordered_set<uint32_t> visited_ti;
  std::unordered_set<uint32_t> visited_pi;
  std::vector<RecedingTriangleSet> receding_sets;
};

struct RenderTriangleGrowthParams {
  float incr;
};

struct RenderTriangleRecedeParams {
  float incr;
  float incr_randomness_range;
  int num_target_sets;
};

bool tick_triangle_growth(RenderTriangleGrowthContext& context,
                          const RenderTriangleGrowthParams& params);

uint32_t tick_triangle_growth(RenderTriangleGrowthContext* ctx,
                              uint16_t* dst_inds, uint32_t max_num_inds, float growth_incr);

void initialize_triangle_growth(RenderTriangleGrowthContext* context,
                                const uint32_t* src_tris, uint32_t num_src_tris,
                                const void* src_p, uint32_t src_stride, uint32_t src_offset,
                                void* dst_p, uint32_t dst_stride, uint32_t dst_offset);

bool tick_triangle_recede(RenderTriangleRecedeContext& context,
                          const RenderTriangleRecedeParams& params);

void initialize_triangle_recede(RenderTriangleRecedeContext* context,
                                const uint32_t* src_tris, uint32_t num_src_tris,
                                const void* src_p, uint32_t src_stride, uint32_t src_offset,
                                void* dst_p, uint32_t dst_stride, uint32_t dst_offset);

}