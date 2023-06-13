#pragma once

namespace grove::gui::layout {
struct Layout;
struct BoxID;
struct BoxSlot;
}

namespace grove::gui::cursor {

struct CursorState;

struct MouseState {
  bool left_down;
  bool right_down;
  float x;
  float y;
  float scroll_x;
  float scroll_y;
};

CursorState* create_cursor_state();
void destroy_cursor_state(CursorState** state);

void begin(CursorState* cursor_state, const MouseState& state, bool disabled);
void evaluate_boxes(CursorState* state, int layout_id, const layout::BoxSlot* boxes, int num_boxes);
void evaluate_boxes(CursorState* state, const layout::Layout* layout);
void end(CursorState* state);

void read_scroll_offsets(const CursorState* state, const layout::BoxID& id, float* x, float* y);
void clear_scroll_offsets(CursorState* state);
bool hovered_over(const CursorState* state, const layout::BoxID& id);
bool hovered_over_any(const CursorState* state);
bool left_down_on(const CursorState* state, const layout::BoxID& id);
bool newly_left_down_on(const CursorState* state, const layout::BoxID& id);
bool newly_left_down(const CursorState* state);
bool newly_left_clicked(const CursorState* state);
bool left_clicked_on(const CursorState* state, const layout::BoxID& id);
bool left_down_on_any(const CursorState* state);
bool right_down_on(const CursorState* state, const layout::BoxID& id);
bool right_clicked_on(const CursorState* state, const layout::BoxID& id);
bool right_down_on_any(const CursorState* state);

const MouseState* read_mouse_state(const CursorState* state);

}