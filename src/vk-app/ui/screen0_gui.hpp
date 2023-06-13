#pragma once

#define GROVE_SCREEN0_GUI_LAYOUT_ID (5)

#include "grove/math/vector.hpp"

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

struct Screen0GUIResult {
  bool close_screen;
};

struct Screen0GUIContext {
  Screen0GUIResult* gui_result;
  Vec2f container_dimensions;
  RenderData* render_data;
  gui::cursor::CursorState& cursor_state;
  bool hidden;
};

void prepare_screen0_gui(const Screen0GUIContext& context);
void evaluate_screen0_gui(const Screen0GUIContext& context);
void render_screen0_gui(const Screen0GUIContext& context);
void terminate_screen0_gui();

}