#pragma once

#include "frustum_cull_types.hpp"
#include "grove/math/Vec3.hpp"
#include "grove/common/ContiguousElementGroupAllocator.hpp"

namespace grove::cull {

struct FrustumCullData {
  uint32_t num_instances() const {
    return uint32_t(instances.size());
  }
  uint32_t num_group_offsets() const {
    return uint32_t(group_offsets.size());
  }

  std::vector<FrustumCullGroupOffset> group_offsets;
  std::vector<FrustumCullInstance> instances;
  ContiguousElementGroupAllocator group_alloc;
  bool modified{};
  bool groups_added_or_removed{};
};

struct FrustumCullGroupHandle {
  uint32_t group_index;
};

struct FrustumCullInstanceDescriptor {
  Vec3f aabb_p0;
  Vec3f aabb_p1;
};

FrustumCullData* get_global_tree_leaves_frustum_cull_data();
FrustumCullData* get_global_branch_nodes_frustum_cull_data();

FrustumCullGroupHandle create_frustum_cull_instance_group(
  FrustumCullData* cull_data, const FrustumCullInstanceDescriptor* instances, uint32_t num_instances);

FrustumCullGroupHandle create_reserved_frustum_cull_instance_group(
  FrustumCullData* cull_data, uint32_t num_instances);

void set_aabb(FrustumCullData* data, FrustumCullGroupHandle group, uint32_t instance,
              const Vec3f& p0, const Vec3f& p1);

void destroy_frustum_cull_instance_group(FrustumCullData* cull_data, FrustumCullGroupHandle handle);

}