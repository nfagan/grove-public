#pragma once

#include "tree_system.hpp"

namespace grove::foliage_occlusion {
struct FoliageOcclusionSystem;
}

namespace grove::cull {
struct FrustumCullData;
}

namespace grove::tree {

struct RenderBranchNodesData;

struct CreateRenderFoliageParams {
  enum class LeavesType {
    Maple,
    Willow,
    ThinCurled,
    Broad,
  };

  LeavesType leaves_type;
  bool init_with_fall_colors;
  bool init_with_zero_global_scale;
};

struct CreateRenderTreeInstanceParams {
  TreeInstanceHandle tree;
  bounds::AccelInstanceHandle query_accel;
  Optional<CreateRenderFoliageParams> create_foliage_components;
  bool enable_branch_nodes_drawable_components;
};

struct RenderTreeInstanceHandle {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(RenderTreeInstanceHandle, id)
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, RenderTreeInstanceHandle, id)
  uint32_t id;
};

struct RenderTreeSystemEvents {
  bool just_created_drawables;
  bool just_reached_leaf_target_scale;
  bool just_reached_leaf_season_change_target;
};

struct RenderTreeSystemLeafGrowthContext {
  float t{1.0f};
  float scale0{};
  float scale1{};
};

struct ReadRenderTreeSystemInstance {
  RenderTreeSystemEvents events;
};

struct RenderTreeSystemInitInfo {};

struct RenderTreeSystemUpdateInfo {
  const tree::TreeSystem* tree_system;
  bounds::BoundsSystem* bounds_system;
  foliage_occlusion::FoliageOcclusionSystem* foliage_occlusion_system;
  cull::FrustumCullData* tree_leaves_frustum_cull_data;
  cull::FrustumCullData* branch_nodes_frustum_cull_data;
  RenderBranchNodesData* render_branch_nodes_data;
  double real_dt;
};

struct RenderTreeSystemUpdateResult {
  int num_just_reached_leaf_season_change_target;
};

struct RenderTreeSystemStats {
  double max_ms_spent_deleting_branches;
  double max_ms_spent_deleting_foliage;
  uint32_t max_num_drawables_destroyed_in_one_frame;
};

struct RenderTreeSystem;

RenderTreeSystem* create_render_tree_system();
void destroy_render_tree_system(RenderTreeSystem** sys);

RenderTreeInstanceHandle create_instance(RenderTreeSystem* sys,
                                         const CreateRenderTreeInstanceParams& params);
void destroy_instance(RenderTreeSystem* sys, RenderTreeInstanceHandle instance);
ReadRenderTreeSystemInstance read_instance(const RenderTreeSystem* sys,
                                           RenderTreeInstanceHandle handle);
const RenderTreeSystemLeafGrowthContext* read_leaf_growth_context(const RenderTreeSystem* sys,
                                                                  RenderTreeInstanceHandle handle);
float read_current_static_leaves_uv_offset(const RenderTreeSystem* sys,
                                           RenderTreeInstanceHandle handle);

void set_hidden(RenderTreeSystem* sys, RenderTreeInstanceHandle inst, bool hide);
void set_all_hidden(RenderTreeSystem* sys, bool hide);

void require_drawables(RenderTreeSystem* sys, RenderTreeInstanceHandle instance);

//  ProceduralTreeComponent/update
void set_leaf_scale_target(RenderTreeSystem* sys, RenderTreeInstanceHandle instance, float target);
void set_leaf_global_scale_fraction(RenderTreeSystem* sys, RenderTreeInstanceHandle instance, float scale01);
void set_static_leaf_uv_offset_target(RenderTreeSystem* sys, RenderTreeInstanceHandle instance, float off);
void increment_static_leaf_uv_osc_time(RenderTreeSystem* sys, RenderTreeInstanceHandle instance, float dt);
void set_frac_fall_target(RenderTreeSystem* sys, RenderTreeInstanceHandle inst, float target);

int get_preferred_foliage_lod(const RenderTreeSystem* sys);
void maybe_set_preferred_foliage_lod(RenderTreeSystem* sys, int lod);

//  After TreeSystem/update, before ProceduralTreeComponent/update
RenderTreeSystemUpdateResult update(RenderTreeSystem* sys, const RenderTreeSystemUpdateInfo& info);
void initialize(RenderTreeSystem* sys, const RenderTreeSystemInitInfo& info);

RenderTreeSystemStats get_stats(const RenderTreeSystem* sys);

namespace debug {
Optional<RenderTreeInstanceHandle> get_ith_instance(const RenderTreeSystem* sys, int i);
}

}