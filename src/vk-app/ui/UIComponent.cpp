#include "UIComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/gl/GLMouse.hpp"
#include "grove/input/MouseButtonTrigger.hpp"

GROVE_NAMESPACE_BEGIN

void UIComponent::initialize() {
  gui::cursor::destroy_cursor_state(&cursor_state);
  cursor_state = gui::cursor::create_cursor_state();
}

void UIComponent::begin_cursor_update(const Vec2f& pos, const Vec2f& scroll,
                                      bool left_pressed, bool right_pressed, bool disabled) {
  gui::cursor::MouseState mouse_state{};
  mouse_state.left_down = left_pressed;
  mouse_state.right_down = right_pressed;
  mouse_state.x = pos.x;
  mouse_state.y = pos.y;
  mouse_state.scroll_x = scroll.x;
  mouse_state.scroll_y = scroll.y;
  gui::cursor::begin(cursor_state, mouse_state, disabled);
}

void UIComponent::end_cursor_update() {
  gui::cursor::end(cursor_state);
}

void UIComponent::terminate() {
  gui::cursor::destroy_cursor_state(&cursor_state);
}

GROVE_NAMESPACE_END
