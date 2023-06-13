#pragma once

namespace grove {
class ProceduralTreeComponent;
class Camera;

namespace tree {
struct TreeSystem;
struct RootsSystem;
}

}

namespace grove::tree::debug {

struct NodeRenderingUpdateInfo {
  const ProceduralTreeComponent& proc_tree_component;
  const tree::TreeSystem* tree_sys;
  const tree::RootsSystem* roots_sys;
  const Camera& camera;
};

void update_fit_node_aabbs(const NodeRenderingUpdateInfo& info);
void render_fit_node_aabbs_gui_dropdown();

}