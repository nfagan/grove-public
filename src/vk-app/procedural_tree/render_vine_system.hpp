#pragma once

#include "grove/common/identifier.hpp"
#include "grove/common/ArrayView.hpp"
#include "grove/math/vector.hpp"
#include <cstdint>

namespace grove::tree {

struct VineRenderNode;
struct VineAttachedToAggregateRenderData;

struct VineRenderSegmentHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(VineRenderSegmentHandle, id)
  uint32_t id;
};

struct VineAttachedToAggregateRenderDescriptor {
  Vec3f wind_aabb_p0;
  Vec3f wind_aabb_p1;
};

struct VineRenderNodeDescriptor {
  uint32_t self_aggregate_index;
  uint32_t child_aggregate_index;
  Vec3f self_p;
  Vec3f child_p;
  Vec3f self_frame_right;
  Vec3f self_frame_up;
  Vec3f child_frame_right;
  Vec3f child_frame_up;
  float self_radius;
  float child_radius;
  Vec4<uint32_t> wind_info0;
  Vec4<uint32_t> wind_info1;
  Vec4<uint32_t> wind_info2;
};

struct RenderVineSystem;

RenderVineSystem* create_render_vine_system();
void destroy_render_vine_system(RenderVineSystem** sys);

VineRenderSegmentHandle
create_vine_render_segment(RenderVineSystem* sys, const VineRenderNodeDescriptor* nodes, int num_nodes,
                           const VineAttachedToAggregateRenderDescriptor* aggregates, int num_aggregates);
void destroy_vine_render_segment(RenderVineSystem* sys, VineRenderSegmentHandle handle);

void set_vine_node_positions(RenderVineSystem* sys, VineRenderSegmentHandle segment, int offset,
                             const VineRenderNodeDescriptor* nodes, int num_nodes);
void set_vine_node_radii(RenderVineSystem* sys, VineRenderSegmentHandle segment, int offset,
                         const VineRenderNodeDescriptor* nodes, int num_nodes, bool broadcast);

ArrayView<const VineRenderNode> read_vine_render_nodes(const RenderVineSystem* sys);
ArrayView<const VineAttachedToAggregateRenderData>
read_vine_attached_to_aggregate_render_data(const RenderVineSystem* sys);
bool test_clear_render_nodes_modified(RenderVineSystem* sys);

}