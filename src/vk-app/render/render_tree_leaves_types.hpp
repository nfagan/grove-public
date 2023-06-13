#pragma once

#include "grove/math/vector.hpp"
#include "grove/common/DistinctRanges.hpp"

namespace grove::foliage {

struct RenderInstance {
  Vec4f translation_forwards_x;
  Vec4f forwards_yz_right_xy;
  Vec4<uint32_t> right_z_instance_group_randomness_unused;
  Vec4f y_rotation_z_rotation_unused;
  Vec4<uint32_t> wind_node_info0;
  Vec4<uint32_t> wind_node_info1;
  Vec4<uint32_t> wind_node_info2;
};

struct RenderInstanceMeta {
  bool enable_fixed_shadow;
};

struct RenderInstanceGroup {
  Vec4<uint32_t> alpha_image_color_image_indices_uv_offset_color_image_mix_unused;
  Vec4f aabb_p0_curl_scale;
  Vec4f aabb_p1_global_scale;
};

struct RenderInstanceGroupMeta {
  float canonical_global_scale;
  float center_uv_offset;
  float uv_osc_time;
  float scale01;
  bool hidden;
};

struct RenderInstanceComponentIndices {
  uint32_t frustum_cull_group;
  uint32_t frustum_cull_instance_index;
  uint32_t is_active;
  uint32_t occlusion_cull_group_cluster_instance_index;
};

struct ComputeLODInstance {
  Vec4f translation_fadeout_allowed;
  Vec4f scale_distance_limits_lod_distance_limits;
};

struct LODDependentData {
  Vec4f scale_fraction_lod_fraction;
};

struct ComputeLODIndex {
  uint32_t index;
};

struct TreeLeavesRenderDataStats {
  uint32_t num_active_instances;
  uint32_t num_inactive_instances;
  uint32_t min_num_instances_in_group;
  uint32_t max_num_instances_in_group;
  double mean_num_instances_per_group;
  uint32_t num_would_overdraw_with_query_pool_size;
  double frac_would_overdraw_with_query_pool_size;
};

struct TreeLeavesRenderData {
  struct InstanceSetIndices {
    uint32_t offset;
    uint32_t count;
    bool in_use;
  };

  uint32_t num_instances() const {
    return uint32_t(instances.size());
  }
  uint32_t num_instance_groups() const {
    return uint32_t(instance_groups.size());
  }
  void reserve_instances(uint32_t n) {
    instances.reserve(n);
    instance_component_indices.reserve(n);
    compute_lod_instances.reserve(n);
    instance_meta.reserve(n);
  }
  void reserve_instance_groups(uint32_t n) {
    instance_group_in_use.reserve(n);
    instance_groups.reserve(n);
    instance_group_meta.reserve(n);
  }
  void acknowledge_instances_modified() {
    modified_instance_ranges.clear();
    modified_instance_ranges_invalidated = false;
    instances_modified = false;
  }

  std::vector<InstanceSetIndices> instance_sets;

  std::vector<RenderInstance> instances;
  std::vector<RenderInstanceComponentIndices> instance_component_indices;
  std::vector<ComputeLODInstance> compute_lod_instances;
  std::vector<RenderInstanceMeta> instance_meta;

  std::vector<uint8_t> instance_group_in_use;
  std::vector<RenderInstanceGroup> instance_groups;
  std::vector<RenderInstanceGroupMeta> instance_group_meta;

  DistinctRanges<uint32_t> modified_instance_ranges;
  bool modified_instance_ranges_invalidated{};
  bool instances_modified{};
  bool instance_groups_modified{};

  uint32_t max_alpha_image_index{};
  uint32_t max_color_image_index{};
};

}