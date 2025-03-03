#pragma once

#include "grove/math/vector.hpp"

#define GROVE_WORLD_GUI_LAYOUT_ID (3)

namespace grove {
class ProceduralTreeComponent;
struct TreeRootsComponent;
class DebugTreeRootsComponent;
class ProceduralFlowerComponent;
struct ArchComponent;
class DebugArchComponent;
class KeyTrigger;
class Camera;
}

namespace grove::gui {

struct WorldGUIContext;

namespace cursor {
struct CursorState;
}

namespace layout {
struct Layout;
}

namespace elements {
struct Elements;
}

struct RenderData;

struct WorldGUIContext {
  Vec2f container_dimensions;
  RenderData* render_data;
  gui::cursor::CursorState& cursor_state;
  const KeyTrigger& key_trigger;
  bool hidden;
  ProceduralTreeComponent& procedural_tree_component;
  TreeRootsComponent& tree_roots_component;
  DebugTreeRootsComponent& db_tree_roots_component;
  ProceduralFlowerComponent& procedural_flower_component;
  ArchComponent& arch_component;
  DebugArchComponent& db_arch_component;
  const Camera& camera;
};

}