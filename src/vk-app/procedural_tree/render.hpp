#pragma once

#include "components.hpp"

namespace grove::tree {

struct RemappedAxisRoot {
//  int root_offset{};
  Vec3f position{};
//  int gravelius_order{};
//  bool found_non_intersecting_child{};
};

struct RemappedAxisRoots {
  using RootInfo =
    std::unordered_map<tree::TreeInternodeID, RemappedAxisRoot, tree::TreeInternodeID::Hash>;
  using EvalAt =
    std::unordered_map<tree::TreeInternodeID, tree::TreeInternodeID, tree::TreeInternodeID::Hash>;

  RootInfo root_info;
  EvalAt evaluate_at;
};

struct WindAxisRootInfo {
  static WindAxisRootInfo missing(int n = 3);
  DynamicArray<Vec4f, 3> info;
};

struct ChildRenderData {
  const tree::Internode* child;
  Vec3f position;
  Vec2f direction;
  float radius;
};

using PackedWindAxisRootInfo = DynamicArray<Vec4<uint32_t>, 3>;

Bounds3f internode_aabb(const tree::Internodes& nodes);
Bounds3f internode_aabb(const tree::Internode* nodes, uint32_t num_nodes);
OBB3f internode_obb(const tree::Internode& node);
void internode_obbs(const tree::Internode* nodes, int num_nodes, OBB3f* dst);
OBB3f internode_relative_obb(const tree::Internode& node, const Vec3f& scale, const Vec3f& off);
OBB3f internode_obb_custom_diameter(const tree::Internode& node, float diameter);
void orient_to_internode_direction(OBB3f* dst, const tree::Internode& inode);
RemappedAxisRoots remap_axis_roots(const tree::Internodes& internodes);
void compute_internode_frames(const tree::Internode* node, int num_nodes, Mat3f* dst);

void copy_diameter_to_lateral_q(Internodes& inodes);
void copy_position_to_render_position(Internodes& inodes);
void mul_lateral_q_diameter_by_length_scale(Internodes& inodes);

void constrain_lateral_child_diameter(tree::Internodes& nodes);
void prefer_larger_axes(tree::Internode* nodes, int num_nodes);

PackedWindAxisRootInfo to_packed_wind_info(const WindAxisRootInfo& parent,
                                           const WindAxisRootInfo& child);

WindAxisRootInfo make_wind_axis_root_info(const tree::Internode& internode,
                                          const tree::Internodes& store,
                                          const tree::AxisRootInfo& axis_root_info,
                                          const RemappedAxisRoots& remapped_roots,
                                          const Bounds3f& tree_aabb);

ChildRenderData get_child_render_data(const tree::Internode& internode,
                                      const tree::Internode* store,
                                      bool allow_branch_to_lateral,
                                      float leaf_tip_radius);

void set_render_position(Internodes& internodes, TreeNodeIndex axis_root_index);
void set_render_length_scale(Internodes& internodes, TreeNodeIndex root_index, float scl);

bool tick_render_axis_growth(Internodes& internodes, RenderAxisGrowthContext& context,
                             float growth_incr);
bool tick_render_axis_death(Internodes& internodes, RenderAxisDeathContext& context,
                            float growth_incr);

tree::RenderAxisDeathContext make_default_render_axis_death_context(const Internodes& internodes);
void initialize_axis_pruning(tree::RenderAxisDeathContext* context,
                             const Internodes& internodes,
                             std::unordered_set<int>&& preserve);

void initialize_axis_render_growth_context(RenderAxisGrowthContext* context,
                                           const Internodes& internodes,
                                           TreeNodeIndex root_index);
void initialize_depth_first_axis_render_growth_context(RenderAxisGrowthContext* context,
                                                       const Internodes& internodes,
                                                       TreeNodeIndex root_index);
bool update_render_growth(Internodes& internodes,
                          const SpawnInternodeParams& spawn_params,
                          RenderAxisGrowthContext& growth_context, float incr);
bool update_render_growth_new_method(Internodes& internodes,
                                     RenderAxisGrowthContext& growth_context, float incr);
bool update_render_growth_depth_first(Internodes& internodes, RenderAxisGrowthContext& growth_context,
                                      float incr, bool* new_axis);
bool update_render_death(Internodes& tree_nodes,
                         const SpawnInternodeParams& spawn_params,
                         RenderAxisDeathContext& death_context, float incr);
bool update_render_death_new_method(Internodes& tree_nodes,
                                    RenderAxisDeathContext& death_context, float incr);
bool update_render_prune(Internodes& tree_nodes,
                         RenderAxisDeathContext& death_context, float incr);

bool update_render_growth_src_diameter_in_lateral_q(
  Internodes& internodes, RenderAxisGrowthContext& context,
  const SpawnInternodeParams& spawn_params, float incr);
bool update_render_death_src_diameter_in_lateral_q(
  Internodes& internodes, RenderAxisDeathContext& context, float incr);

}