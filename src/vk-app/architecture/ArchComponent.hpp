#pragma once

namespace grove {

template <typename T>
struct OBB3;

struct Ray;

namespace tree {
struct TreeSystem;
struct RootsSystem;
struct ProjectedNodesSystem;
struct VineSystem;
struct RenderVineSystem;
struct Internode;
}

namespace bounds {
struct AccelInstanceHandle;
struct BoundsSystem;
struct RadiusLimiter;
struct ElementTag;
struct RadiusLimiterElementTag;
}

struct ArchComponent;
class ArchRenderer;

struct ArchComponentInitInfo {
  ArchRenderer* renderer;
  const bounds::ElementTag& arch_bounds_element_tag;
  const bounds::RadiusLimiterElementTag& arch_radius_limiter_element_tag;
};

struct ArchComponentUpdateInfo {
  double real_dt;
  ArchRenderer* renderer;
  tree::TreeSystem* tree_system;
  tree::RootsSystem* roots_system;
  tree::ProjectedNodesSystem* projected_nodes_system;
  tree::VineSystem* vine_system;
  tree::RenderVineSystem* render_vine_system;
  const bounds::AccelInstanceHandle& accel_handle;
  bounds::BoundsSystem* bounds_system;
  const OBB3<float>& debug_collider_bounds;
  bounds::RadiusLimiter* radius_limiter;
  const Ray& mouse_ray;
  bool left_clicked;
  const tree::Internode* proj_internodes;
  int num_proj_internodes;
};

struct ArchComponentParams {
  bool extrude_from_parent;
  float extrude_theta;
  bool disable_tentative_bounds_highlight;
};

struct ArchComponentExtrudeInfo {
  bool growing;
  bool receding;
  bool can_extrude;
  bool can_recede;
  bool waiting_on_trees_or_roots_to_finish_pruning;
};

ArchComponent* get_global_arch_component();
void initialize_arch_component(ArchComponent* component, const ArchComponentInitInfo& info);
void update_arch_component(ArchComponent* component, const ArchComponentUpdateInfo& info);
void render_arch_component_gui(ArchComponent* component);
ArchComponentParams get_arch_component_params(const ArchComponent* component);
void set_arch_component_params(ArchComponent* component, const ArchComponentParams& params);
void set_arch_component_need_extrude_structure(ArchComponent* component);
void set_arch_component_need_recede_structure(ArchComponent* component);
void set_arch_component_need_project_onto_structure(ArchComponent* component);

ArchComponentExtrudeInfo get_arch_component_extrude_info(const ArchComponent* component);

}