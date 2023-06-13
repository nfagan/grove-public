#pragma once

#include "../bounds/bounds_system.hpp"

namespace grove {

class BoundsComponent;

namespace tree {
struct TreeSystem;
struct RenderTreeSystem;
struct ProjectedNodesSystem;
struct RootsSystem;
struct VineSystem;
}

struct SystemsGUIUpdateResult {
  struct ModifyDebugInstance {
    bounds::AccelInstanceHandle target;
    bool intersect_drawing_enabled;
    Vec3f intersect_bounds_scale;
  };

  Optional<bounds::AccelInstanceHandle> need_rebuild;
  Optional<bounds::CreateAccelInstanceParams> default_build_params;
  Optional<ModifyDebugInstance> modify_debug_instance;
  bool close;
};

struct SystemsGUIRenderInfo {
  bounds::BoundsSystem* bounds_system;
  const bounds::AccelInstanceHandle* accel_instances;
  int num_accel_instances;
  const BoundsComponent& bounds_component;
  const tree::TreeSystem& tree_system;
  tree::RenderTreeSystem& render_tree_system;
  const tree::ProjectedNodesSystem& projected_nodes_system;
  const tree::RootsSystem& roots_system;
  const tree::VineSystem& vine_system;
};

class SystemsGUI {
public:
  SystemsGUIUpdateResult render(const SystemsGUIRenderInfo& info);

public:
  bounds::AccessorID bounds_accessor{bounds::AccessorID::create()};
  int debug_ith_render_tree_instance{};
  float debug_render_tree_instance_global_leaf_scale{1.0f};
};

}