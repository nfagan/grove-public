#include "gui_cursor.hpp"
#include "gui_layout.hpp"
#include "gui_layout_private.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/common.hpp"
#include "grove/common/platform.hpp"
#include "grove/math/Vec2.hpp"
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace {

struct OverBox {
  gui::layout::BoxID id;
  uint16_t depth;
};

} //  anon

namespace gui::cursor {

struct CursorState {
  bool ended{};
  bool disabled{};
  MouseState state{};
  bool newly_left_down{};
  bool newly_left_clicked{};
  bool newly_right_down{};
  Optional<OverBox> over_box;
  Optional<OverBox> scroll_over_box;
  Optional<OverBox> hovered_over_box;
  Optional<OverBox> left_mouse_down_on_box;
  Optional<OverBox> newly_left_mouse_down_on_box;
  Optional<layout::BoxID> left_clicked_on;
  Optional<OverBox> right_mouse_down_on_box;
  Optional<layout::BoxID> right_clicked_on;
  std::unordered_map<gui::layout::BoxID, Vec2f, gui::layout::BoxID::Hash> scroll_offsets;
};

} //  gui::cursor

gui::cursor::CursorState* gui::cursor::create_cursor_state() {
  auto* res = new CursorState();
  res->ended = true;
  return res;
}

void gui::cursor::destroy_cursor_state(CursorState** state) {
  delete *state;
  *state = nullptr;
}

void gui::cursor::begin(CursorState* cursor_state, const MouseState& state, bool disabled) {
  assert(cursor_state->ended);
  cursor_state->disabled = disabled;
  cursor_state->newly_left_clicked = false;
  cursor_state->newly_left_down = false;
  cursor_state->newly_right_down = false;
  if (!disabled) {
    if (cursor_state->state.left_down && !state.left_down) {
      cursor_state->newly_left_clicked = true;
    }
    cursor_state->newly_left_down = state.left_down && !cursor_state->state.left_down;
    cursor_state->newly_right_down = state.right_down && !cursor_state->state.right_down;
  }
  cursor_state->state = state;
#ifdef GROVE_WIN
  cursor_state->state.scroll_x *= 4.0f;
  cursor_state->state.scroll_y *= 4.0f;
#endif
  cursor_state->state.scroll_x *= disabled ? 0.0f : 4.0f;
  cursor_state->state.scroll_y *= disabled ? 0.0f : 4.0f;
  cursor_state->over_box = NullOpt{};
  cursor_state->scroll_over_box = NullOpt{};
  cursor_state->hovered_over_box = NullOpt{};
  cursor_state->left_clicked_on = NullOpt{};
  cursor_state->right_clicked_on = NullOpt{};
  cursor_state->newly_left_mouse_down_on_box = NullOpt{};
  cursor_state->ended = false;
}

void gui::cursor::evaluate_boxes(CursorState* state, const layout::Layout* layout) {
  const layout::BoxSlot* beg = layout::read_box_slot_begin(layout);
  evaluate_boxes(state, layout::get_id(layout), beg, layout::total_num_boxes(layout));
}

void gui::cursor::evaluate_boxes(CursorState* state, int layout_id, const layout::BoxSlot* boxes, int num_boxes) {
  assert(!state->ended);

  if (state->disabled) {
    return;
  }

  const float mx = state->state.x;
  const float my = state->state.y;

  Optional<int> over;
  for (int i = 0; i < num_boxes; i++) {
    auto& box = boxes[i];
    auto box_id = layout::BoxID::create(layout_id, i);
    const bool within = mx >= box.clip_x0 && mx < box.clip_x1 &&
                        my >= box.clip_y0 && my < box.clip_y1;
    if (within) {
      if (box.events.click() &&
         (!state->hovered_over_box || state->hovered_over_box.value().depth < box.depth)) {
        state->hovered_over_box = OverBox{box_id, box.depth};
      }

      if (!state->over_box || state->over_box.value().depth < box.depth) {
        state->over_box = OverBox{box_id, box.depth};
        over = i;
      }

      if (box.events.scroll() &&
          (!state->scroll_over_box || state->scroll_over_box.value().depth < box.depth)) {
        state->scroll_over_box = OverBox{box_id, box.depth};
      }
    }
  }

  if (over && state->hovered_over_box &&
      state->hovered_over_box.value().id != state->over_box.value().id &&
      !boxes[over.value()].events.pass()) {
    //  Click target is blocked by box covering it.
    state->hovered_over_box = NullOpt{};
  }
}

void gui::cursor::read_scroll_offsets(const CursorState* state, const layout::BoxID& id,
                                      float* x, float* y) {
//  assert(state->ended);
  auto it = state->scroll_offsets.find(id);
  if (it == state->scroll_offsets.end()) {
    if (x) {
      *x = 0.0f;
    }
    if (y) {
      *y = 0.0f;
    }
  } else {
    if (x) {
      *x = it->second.x;
    }
    if (y) {
      *y = it->second.y;
    }
  }
}

void gui::cursor::clear_scroll_offsets(CursorState* state) {
  state->scroll_offsets.clear();
}

bool gui::cursor::hovered_over(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->hovered_over_box && state->hovered_over_box.value().id == id;
}

bool gui::cursor::hovered_over_any(const CursorState* state) {
  return state->hovered_over_box.has_value();
}

bool gui::cursor::left_down_on(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->left_mouse_down_on_box && state->left_mouse_down_on_box.value().id == id;
}

bool gui::cursor::newly_left_down_on(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->newly_left_mouse_down_on_box && state->newly_left_mouse_down_on_box.value().id == id;
}

bool gui::cursor::newly_left_down(const CursorState* state) {
  return state->newly_left_down;
}

bool gui::cursor::newly_left_clicked(const CursorState* state) {
  return state->newly_left_clicked;
}

bool gui::cursor::left_down_on_any(const CursorState* state) {
  assert(state->ended);
  return state->left_mouse_down_on_box;
}

bool gui::cursor::left_clicked_on(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->left_clicked_on && state->left_clicked_on.value() == id;
}

bool gui::cursor::right_down_on(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->right_mouse_down_on_box && state->right_mouse_down_on_box.value().id == id;
}

bool gui::cursor::right_down_on_any(const CursorState* state) {
  assert(state->ended);
  return state->right_mouse_down_on_box;
}

bool gui::cursor::right_clicked_on(const CursorState* state, const layout::BoxID& id) {
  assert(state->ended);
  return state->right_clicked_on && state->right_clicked_on.value() == id;
}

void gui::cursor::end(CursorState* state) {
  if (state->scroll_over_box) {
    auto id = state->scroll_over_box.value().id;
    if (state->scroll_offsets.count(id) == 0) {
      state->scroll_offsets[id] = {};
    }

    auto& off = state->scroll_offsets.at(id);
    off.x += state->state.scroll_x;
    off.x = std::min(off.x, 0.0f);
    off.y += state->state.scroll_y;
    off.y = std::min(off.y, 0.0f);
  }

  //  Left
  if (state->left_mouse_down_on_box && !state->state.left_down) {
    auto down_id = state->left_mouse_down_on_box.value().id;
    state->left_mouse_down_on_box = NullOpt{};
    if (state->hovered_over_box && state->hovered_over_box.value().id == down_id) {
      state->left_clicked_on = down_id;
    }

  } else if (!state->left_mouse_down_on_box && state->hovered_over_box && state->newly_left_down) {
    state->left_mouse_down_on_box = state->hovered_over_box;
    state->newly_left_mouse_down_on_box = state->hovered_over_box;
  }

  //  Right
  if (state->right_mouse_down_on_box && !state->state.right_down) {
    auto down_id = state->right_mouse_down_on_box.value().id;
    state->right_mouse_down_on_box = NullOpt{};
    if (state->hovered_over_box && state->hovered_over_box.value().id == down_id) {
      state->right_clicked_on = down_id;
    }

  } else if (!state->right_mouse_down_on_box && state->hovered_over_box && state->newly_right_down) {
    state->right_mouse_down_on_box = state->hovered_over_box;
  }

  state->ended = true;
}

const gui::cursor::MouseState* gui::cursor::read_mouse_state(const CursorState* state) {
  return &state->state;
}

GROVE_NAMESPACE_END
