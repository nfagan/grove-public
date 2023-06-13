#pragma once

#include "roots_components.hpp"
#include "grove/common/identifier.hpp"

namespace grove {
template <typename T>
class Optional;
}

namespace grove::tree {

struct RootsSystemStats {
  int num_instances;
  int num_growing_instances;
  int max_num_new_branch_infos;
};

struct RootsEvents {
  bool grew;
  bool receded;
  bool pruned;
  bool just_finished_pruning;
};

struct RootsNewBranchInfo {
  Vec3f position;
};

enum class TreeRootsState {
  Idle = 0,
  PendingInit,
  Growing,
  Alive,
  Pruning,
  Dying,
  Dead,
  WillDestroy
};

struct RootsInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RootsInstanceHandle, id)
  uint32_t id;
};

struct RootsSystemUpdateInfo {
  bounds::RadiusLimiter* radius_limiter;
  double real_dt;
};

struct RootsSystemUpdateResult {
  int num_new_branches;
  const RootsNewBranchInfo* new_branch_infos;
  int num_new_branch_infos;
};

struct CreateRootsInstanceParams {
  Vec3f origin;
  Vec3f init_direction;
};

struct ReadRootsInstance {
  const TreeRoots* roots;
  RootsEvents events;
  TreeRootsState state;
};

struct RootsSystem;

RootsSystem* create_roots_system(bounds::RadiusLimiterElementTag roots_element_tag);
RootsSystemUpdateResult update_roots_system(RootsSystem* sys, const RootsSystemUpdateInfo& info);
void end_update_roots_system(RootsSystem* sys);
void destroy_roots_system(RootsSystem** sys);

RootsInstanceHandle create_roots_instance(RootsSystem* sys, const CreateRootsInstanceParams& params);
ReadRootsInstance read_roots_instance(const RootsSystem* sys, RootsInstanceHandle inst);
int collect_roots_instance_handles(const RootsSystem* sys, RootsInstanceHandle* dst, int max_dst);

Optional<RootsInstanceHandle> lookup_roots_instance_by_radius_limiter_aggregate_id(
  const RootsSystem* sys, bounds::RadiusLimiterAggregateID id);

bool can_start_dying(const RootsSystem* sys, RootsInstanceHandle inst);
void start_dying(RootsSystem* sys, RootsInstanceHandle inst);
bool can_destroy_roots_instance(const RootsSystem* sys, RootsInstanceHandle inst);
void destroy_roots_instance(RootsSystem* sys, RootsInstanceHandle inst);
bool can_start_pruning(const RootsSystem* sys, RootsInstanceHandle inst);
void start_pruning_roots(RootsSystem* sys, RootsInstanceHandle inst,
                         std::vector<int>&& pruned_dst_to_src,
                         std::vector<tree::TreeRootNodeIndices>&& pruned_node_indices);

void set_global_growth_rate_scale(RootsSystem* sys, float s);
void set_global_attractor_point(RootsSystem* sys, const Vec3f& p);
void set_global_attractor_point_scale(RootsSystem* sys, float s);
void set_attenuate_growth_rate_by_spectral_fraction(RootsSystem* sys, bool atten);
void set_spectral_fraction(RootsSystem* sys, float s);
void set_global_p_spawn_lateral_branch(RootsSystem* sys, double p);
void set_prefer_global_p_spawn_lateral_branch(RootsSystem* sys, bool pref);

bounds::RadiusLimiterElementTag get_roots_radius_limiter_element_tag(const RootsSystem* sys);

RootsSystemStats get_stats(const RootsSystem* sys);

}