#pragma once

#include "grove/common/identifier.hpp"
#include "grove/math/Vec3.hpp"

namespace grove {
template <typename T>
struct Vec3;
}

namespace grove::bounds {
struct BoundsSystem;
struct AccelInstanceHandle;
struct ElementTag;
}

namespace grove::tree {

struct VineSystem;
struct RenderVineSystem;
struct TreeSystem;
struct TreeInstanceHandle;
struct Internode;

struct VineNode {
  bool has_medial_child() const {
    return medial_child != -1;
  }
  bool has_lateral_child() const {
    return lateral_child != -1;
  }
  Vec3f decode_attached_surface_normal() const;

  Vec3f position;
  Vec3f direction;
  float radius;
  int parent;
  int medial_child;
  int lateral_child;
  int attached_node_index;
  Vec3<uint8_t> attached_surface_normal;
};

struct ReadVineSegment {
  const VineNode* nodes;  //  might be null
  int node_beg;
  int node_end;
  bool finished_growing;
  uint32_t maybe_associated_tree_instance_id;
};

struct VineSystemStats {
  int num_instances;
  int num_segments;
  int num_nodes;
};

struct VineSystemUpdateInfo {
  TreeSystem* tree_system;
  RenderVineSystem* render_vine_system;
  bounds::BoundsSystem* bounds_system;
  const bounds::AccelInstanceHandle& accel_handle;
  const bounds::ElementTag& arch_bounds_element_tag;
  double real_dt;
};

struct VineSegmentHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(VineSegmentHandle, id)
  uint32_t id;
};

struct VineInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(VineInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

struct VineSystemTryToJumpToNearbyTreeParams {
  bool use_initial_offset;
  Vec3f initial_offset;
};

VineSystem* create_vine_system();
void destroy_vine_system(VineSystem** sys);
void update_vine_system(VineSystem* sys, const VineSystemUpdateInfo& info);
VineInstanceHandle create_vine_instance(VineSystem* sys, float radius);
void destroy_vine_instance(VineSystem* sys, VineInstanceHandle inst);
bool vine_exists(const VineSystem* sys, VineInstanceHandle inst);

VineSegmentHandle start_new_vine_on_tree(VineSystem* sys, VineInstanceHandle inst,
                                         TreeInstanceHandle tree, float spiral_theta);
VineSegmentHandle emplace_vine_from_internodes(VineSystem* sys, RenderVineSystem* render_vine_sys,
                                               VineInstanceHandle inst, const Internode* internodes,
                                               const Vec3f* surface_ns, int num_internodes);
void try_to_jump_to_nearby_tree(VineSystem* sys, VineInstanceHandle inst, VineSegmentHandle segment,
                                const VineSystemTryToJumpToNearbyTreeParams& params);
float get_global_growth_rate_scale(const VineSystem* sys);
void set_global_growth_rate_scale(VineSystem* sys, float s);
void set_growth_rate_scale(VineSystem* sys, VineInstanceHandle inst, float s);

ReadVineSegment
read_vine_segment(const VineSystem* sys, VineInstanceHandle inst, VineSegmentHandle seg);

VineSystemStats get_stats(const VineSystem* sys);

}