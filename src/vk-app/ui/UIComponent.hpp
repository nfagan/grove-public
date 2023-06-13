#pragma once

#include "grove/gui/gui_cursor.hpp"
#include "grove/math/Vec2.hpp"

namespace grove {

class GLMouse;
class MouseButtonTrigger;

class UIComponent {
public:
  void initialize();
  void begin_cursor_update(const Vec2f& pos, const Vec2f& scroll, bool left_pressed, bool right_pressed,
                           bool disabled);
  void end_cursor_update();
  void terminate();

public:
  gui::cursor::CursorState* cursor_state{};
};

}