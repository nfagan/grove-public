#include "frustum_cull_data.hpp"
#include "frustum_cull_types.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace cull;

FrustumCullGroupHandle create_frustum_cull_instance_group(
  FrustumCullData* sys, const FrustumCullInstanceDescriptor* instances, uint32_t num_instances,
  bool do_reserve) {
  //
  const uint32_t curr_num_insts = sys->num_instances();
  ContiguousElementGroupAllocator::ElementGroupHandle gh{};
  (void) sys->group_alloc.reserve(num_instances, &gh);

  sys->group_offsets.resize(sys->group_alloc.num_groups());
  sys->group_offsets[gh.index] = FrustumCullGroupOffset{curr_num_insts};
  sys->instances.resize(curr_num_insts + num_instances);

  if (do_reserve) {
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& dst_inst = sys->instances[i + curr_num_insts];
      dst_inst = {};
      dst_inst.aabb_p0 = {};
      dst_inst.aabb_p1 = Vec4f{1.0f, 1.0f, 1.0f, 0.0f};
    }
  } else {
    for (uint32_t i = 0; i < num_instances; i++) {
      auto& src_inst = instances[i];
      auto& dst_inst = sys->instances[i + curr_num_insts];
      dst_inst = {};
      dst_inst.aabb_p0 = Vec4f{src_inst.aabb_p0, 0.0f};
      dst_inst.aabb_p1 = Vec4f{src_inst.aabb_p1, 0.0f};
    }
  }

  sys->modified = true;
  sys->groups_added_or_removed = true;

  FrustumCullGroupHandle result{};
  result.group_index = gh.index;
  return result;
}

void destroy_frustum_cull_instance_group(FrustumCullData* sys, FrustumCullGroupHandle group) {
  ContiguousElementGroupAllocator::ElementGroupHandle gh{group.group_index};
  sys->group_alloc.release(gh);

  uint32_t new_num_instances{};
  ContiguousElementGroupAllocator::Movement movement{};
  (void) sys->group_alloc.arrange_implicit(&movement, &new_num_instances);
  movement.apply(sys->instances.data(), sizeof(FrustumCullInstance));
  sys->instances.resize(new_num_instances);

  assert(sys->group_alloc.num_groups() == sys->group_offsets.size());
  uint32_t off{};
  for (auto* it = sys->group_alloc.read_group_begin();
       it != sys->group_alloc.read_group_end();
       ++it) {
    sys->group_offsets[off++] = FrustumCullGroupOffset{it->offset};
  }

  sys->modified = true;
  sys->groups_added_or_removed = true;
}

struct {
  FrustumCullData tree_leaves_frustum_cull_data;
  FrustumCullData branch_nodes_frustum_cull_data;
} globals;

} //  anon

FrustumCullData* cull::get_global_tree_leaves_frustum_cull_data() {
  return &globals.tree_leaves_frustum_cull_data;
}

FrustumCullData* cull::get_global_branch_nodes_frustum_cull_data() {
  return &globals.branch_nodes_frustum_cull_data;
}

FrustumCullGroupHandle cull::create_frustum_cull_instance_group(
  FrustumCullData* sys, const FrustumCullInstanceDescriptor* instances, uint32_t num_instances) {
  //
  return grove::create_frustum_cull_instance_group(sys, instances, num_instances, false);
}

FrustumCullGroupHandle cull::create_reserved_frustum_cull_instance_group(
  FrustumCullData* sys, uint32_t num_instances) {
  //
  return grove::create_frustum_cull_instance_group(sys, nullptr, num_instances, true);
}

void cull::set_aabb(FrustumCullData* data, FrustumCullGroupHandle gh,
                    uint32_t instance, const grove::Vec3f& p0, const grove::Vec3f& p1) {
  assert(gh.group_index < data->num_group_offsets());
  const auto* group = data->group_alloc.read_group(
    ContiguousElementGroupAllocator::ElementGroupHandle{gh.group_index});
  assert(instance < group->count);

  uint32_t inst_ind = group->offset + instance;
  assert(inst_ind < uint32_t(data->instances.size()));

  auto& inst = data->instances[inst_ind];
  inst.aabb_p0 = Vec4f{p0, 0.0f};
  inst.aabb_p1 = Vec4f{p1, 0.0f};
  data->modified = true;
}

void cull::destroy_frustum_cull_instance_group(FrustumCullData* sys, FrustumCullGroupHandle handle) {
  grove::destroy_frustum_cull_instance_group(sys, handle);
}

GROVE_NAMESPACE_END
