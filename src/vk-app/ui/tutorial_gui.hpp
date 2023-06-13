#pragma once

#define GROVE_TUTORIAL_GUI_LAYOUT_ID (7)

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

struct TutorialGUIResult {
  bool close_screen;
};

struct TutorialGUIContext {
  TutorialGUIResult* gui_result;
  Vec2f container_dimensions;
  RenderData* render_data;
  gui::cursor::CursorState& cursor_state;
  bool hidden;
};

void jump_to_first_tutorial_gui_slide();
void prepare_tutorial_gui(const TutorialGUIContext& context);
void evaluate_tutorial_gui(const TutorialGUIContext& context);
void render_tutorial_gui(const TutorialGUIContext& context);
void terminate_tutorial_gui();

}