#pragma once

#include "grove/common/ContiguousElementGroupAllocator.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/util.hpp"

namespace grove::tree {

namespace tree_detail {

inline uint16_t encode_dir_component_u16(float v) {
  v = clamp(v, -1.0f, 1.0f);
  return uint16_t(clamp((v * 0.5f + 0.5f) * float(0xffff), 0.0f, float(0xffff)));
}

inline uint32_t encode_dir_components_u32(float c, float s) {
  return (uint32_t(encode_dir_component_u16(c)) << 16u) | uint32_t(encode_dir_component_u16(s));
}

} //  tree_detail

struct RenderBranchNodeDynamicData {
  void set_position_and_radii(const Vec3f& self_p, float self_r,
                              const Vec3f& child_p, float child_r) {
    self_p_self_r = Vec4f{self_p, self_r};
    child_p_child_r = Vec4f{child_p, child_r};
  }

  Vec4f self_p_self_r;
  Vec4f child_p_child_r;
};

struct RenderBranchNodeStaticData {
  void set_directions(const Vec3f& self_right, const Vec3f& self_up,
                      const Vec3f& child_right, const Vec3f& child_up) {
    Vec4<uint32_t> dir0{};
    dir0.x = tree_detail::encode_dir_components_u32(child_right.x, self_right.x);
    dir0.y = tree_detail::encode_dir_components_u32(child_right.y, self_right.y);
    dir0.z = tree_detail::encode_dir_components_u32(child_right.z, self_right.z);
    dir0.w = tree_detail::encode_dir_components_u32(child_up.x, self_up.x);

    Vec4<uint32_t> dir1{};
    dir1.x = tree_detail::encode_dir_components_u32(child_up.y, self_up.y);
    dir1.y = tree_detail::encode_dir_components_u32(child_up.z, self_up.z);

    directions0 = dir0;
    directions1 = dir1;
  }

  Vec4<uint32_t> directions0;
  Vec4<uint32_t> directions1;
  Vec4<uint32_t> aggregate_index_unused;
};

struct RenderWindBranchNodeStaticData {
  RenderBranchNodeStaticData base;
  Vec4<uint32_t> wind_info0;
  Vec4<uint32_t> wind_info1;
  Vec4<uint32_t> wind_info2;
};

struct RenderBranchNodeAggregate {
  Vec4f aabb_p0_unused;
  Vec4f aabb_p1_unused;
};

struct RenderBranchNodeLODData {
  void set_one_based_cull_group_and_zero_based_instance(uint16_t group_one_based, uint16_t instance) {
#ifdef GROVE_DEBUG
    if (group_one_based == 0) {
      assert(instance == 0);
    }
#endif
    cull_group_and_instance = uint32_t(group_one_based) | (uint32_t(instance) << 16u);
  }
  void set_is_active(bool active) {
    is_active = uint32_t(active);
  }

  uint32_t cull_group_and_instance;  //  1 based cull group, 0 means disabled
  uint32_t is_active;
  uint32_t unused_reserved2;
  uint32_t unused_reserved3;
};

struct RenderBranchNodesData {
public:
  template <typename T>
  struct InstanceSet {
    uint32_t num_aggregates() const {
      return uint32_t(aggregates.size());
    }
    uint32_t num_instances() const {
      return uint32_t(dynamic_instances.size());
    }
    void reserve(uint32_t num_instances) {
      dynamic_instances.reserve(num_instances);
      static_instances.reserve(num_instances);
      lod_data.reserve(num_instances);
    }

    ContiguousElementGroupAllocator alloc;
    std::vector<RenderBranchNodeDynamicData> dynamic_instances;
    std::vector<T> static_instances;
    std::vector<RenderBranchNodeAggregate> aggregates;
    std::vector<RenderBranchNodeLODData> lod_data;

    bool static_instances_modified{};
    bool dynamic_instances_modified{};
    bool lod_data_modified{};
    bool lod_data_potentially_invalidated{};
    bool aggregates_modified{};
  };

  using BaseSet = InstanceSet<RenderBranchNodeStaticData>;
  using WindSet = InstanceSet<RenderWindBranchNodeStaticData>;

public:
  void reserve(uint32_t num_instances_per_set) {
    base_set.reserve(num_instances_per_set);
    wind_set.reserve(num_instances_per_set);
  }

public:
  BaseSet base_set;
  WindSet wind_set;
};

}