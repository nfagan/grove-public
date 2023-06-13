#pragma once

#include "../render/ProceduralTreeRootsRenderer.hpp"

namespace grove {
class Terrain;
}

namespace grove::ls {

struct LSystemComponent;

struct LSystemComponentUpdateInfo {
  ProceduralTreeRootsRenderer& roots_renderer;
  const ProceduralTreeRootsRenderer::AddResourceContext& roots_renderer_context;
  const Terrain& terrain;
};

LSystemComponent* create_lsystem_component();
void destroy_lsystem_component(LSystemComponent** comp);

void update_lsystem_component(LSystemComponent* comp, const LSystemComponentUpdateInfo& info);
void render_lsystem_component_gui(LSystemComponent* comp);

}