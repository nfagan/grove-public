#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/math/OBB3.hpp"
#include "grove/math/Frustum.hpp"
#include "grove/common/ContiguousElementGroupAllocator.hpp"

namespace grove::foliage_occlusion {

struct FoliageOcclusionSystem;

struct ClusterGroupHandle {
  bool is_valid() const {
    return element_group != ContiguousElementGroupAllocator::invalid_element_group;
  }

  ContiguousElementGroupAllocator::ElementGroupHandle element_group{
    ContiguousElementGroupAllocator::invalid_element_group
  };
};

struct ClusterInstanceDescriptor {
  Vec3f p;
  Vec3f x;
  Vec3f n;
  Vec2f s;
  uint32_t associated_render_instance;
};

struct ClusterDescriptor {
  static constexpr uint32_t max_num_instances = 8;

  OBB3f bounds;
  ClusterInstanceDescriptor instances[max_num_instances];
  uint32_t num_instances;
};

struct CheckOccludedParams {
  float cull_distance_threshold;
  float fade_back_in_distance_threshold;
  bool fade_back_in_only_when_below_distance_threshold;
  float min_intersect_area_fraction;
  float tested_instance_scale;
  Vec3f camera_position;
  Mat4f camera_projection_view;
  Frustum camera_frustum;
  int interval;
  float fade_in_time_scale;
  float fade_out_time_scale;
  float cull_time_scale;
  bool disable_cpu_check;
  int max_num_steps;
};

struct CheckOccludedResult {
  uint32_t num_newly_tested;
  uint32_t num_newly_occluded;
  uint32_t total_num_occluded;
  uint32_t num_passed_frustum_cull;
  float ms;
};

struct DebugDrawFoliageOcclusionSystemParams {
  Vec3f mouse_ro;
  Vec3f mouse_rd;
  bool draw_occluded;
  bool draw_cluster_bounds;
  bool colorize_instances;
};

struct UpdateOcclusionSystemResult {
  bool data_structure_modified;
  bool clusters_modified;
};

struct OcclusionSystemStats {
  uint32_t num_grid_lists;
  uint32_t num_clusters;
};

FoliageOcclusionSystem* create_foliage_occlusion_system();
void destroy_foliage_occlusion_system(FoliageOcclusionSystem** sys);
UpdateOcclusionSystemResult update_foliage_occlusion_system(FoliageOcclusionSystem* sys);
OcclusionSystemStats get_foliage_occlusion_system_stats(const FoliageOcclusionSystem* sys);

void remove_cluster_group(FoliageOcclusionSystem* sys, const ClusterGroupHandle& gh);
ClusterGroupHandle insert_cluster_group(FoliageOcclusionSystem* sys,
                                        const ClusterDescriptor* cluster_desc,
                                        uint32_t num_clusters);

bool renderer_check_is_culled_instance_binary(const FoliageOcclusionSystem* sys, uint32_t maybe_group_handle,
                                              uint32_t cluster_offset, uint8_t instance_index);
bool renderer_check_is_culled_instance_fade_in_out(const FoliageOcclusionSystem* sys, uint32_t maybe_group_handle,
                                                   uint32_t cluster_offset, uint8_t instance_index,
                                                   float* scale01);

CheckOccludedResult check_occluded(FoliageOcclusionSystem* sys, const CheckOccludedParams& params);
CheckOccludedResult update_clusters(FoliageOcclusionSystem* sys, double real_dt,
                                    const CheckOccludedParams& params);
void clear_culled(FoliageOcclusionSystem* sys);
uint32_t total_num_instances(const FoliageOcclusionSystem* sys);
void debug_draw(FoliageOcclusionSystem* sys, const DebugDrawFoliageOcclusionSystemParams& params);

}