#pragma once

#define GROVE_MENU_GUI_LAYOUT_ID (4)

#include "grove/math/vector.hpp"

namespace grove {
class AudioComponent;
}

namespace grove::vk {
struct GraphicsContext;
}

namespace grove::gfx {
struct QualityPresetSystem;
}

namespace grove::gui {

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
struct BoxDrawList;

struct MenuGUIResult {
  bool close_gui;
  bool quit_app;
  bool enable_tutorial_gui;
};

struct MenuGUIContext {
  MenuGUIResult* gui_result;
  void* gui_data;
  Vec2f container_dimensions;
  RenderData* render_data;
  gui::cursor::CursorState& cursor_state;
  AudioComponent& audio_component;
  vk::GraphicsContext& vk_graphics_context;
  gfx::QualityPresetSystem& graphics_quality_preset_system;
  bool hidden;
};

}