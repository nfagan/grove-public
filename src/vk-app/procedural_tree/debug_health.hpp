#pragma once

namespace grove {

class ProceduralTreeComponent;

}

namespace grove::tree {

struct ResourceSpiralAroundNodesSystem;

struct DebugHealthUpdateInfo {
  ProceduralTreeComponent& proc_tree_component;
  ResourceSpiralAroundNodesSystem* resource_spiral_sys;
};

void update_debug_health(const DebugHealthUpdateInfo& info);
void render_debug_health_gui();

}