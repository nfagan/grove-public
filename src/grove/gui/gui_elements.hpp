#pragma once

#include "gui_layout.hpp"
#include "grove/common/Optional.hpp"
#include <vector>

namespace grove::gui::cursor {
struct CursorState;
}

namespace grove::gui::elements {

struct StatefulButtonData;

using DropdownCallback = void(int, void*);
using SliderDragCallback = void(float, void*);
using CheckboxCallback = void(bool, void*);
using ClickCallback = void(void*);
using StatefulClickCallback = void(void*, const StatefulButtonData&);

struct StatefulButtonData {
  static StatefulButtonData from_uint32(uint32_t v) {
    StatefulButtonData res{};
    memcpy(res.bytes, &v, sizeof(uint32_t));
    return res;
  }

  static StatefulButtonData from_2_uint32(uint32_t a, uint32_t b) {
    static_assert(sizeof(uint32_t) == 4);
    StatefulButtonData res{};
    memcpy(res.bytes, &a, sizeof(uint32_t));
    memcpy(&res.bytes[4], &b, sizeof(uint32_t));
    return res;
  }

  uint32_t as_uint32() const {
    uint32_t res{};
    memcpy(&res, bytes, sizeof(uint32_t));
    return res;
  }

  void as_2_uint32(uint32_t* a, uint32_t* b) const {
    static_assert(sizeof(uint32_t) == 4);
    memcpy(a, &bytes, sizeof(uint32_t));
    memcpy(b, &bytes[4], sizeof(uint32_t));
  }

  unsigned char bytes[8];
};

struct DropdownData {
  int option;
  bool open;
};

struct SliderDataFlags {
  static constexpr uint8_t Dragging = uint8_t(1u);
  static constexpr uint8_t Stepped = uint8_t(1u << 1u);
};

struct SliderData {
public:
  bool is_stepped() const {
    return flags & SliderDataFlags::Stepped;
  }

  void set_stepped(bool v) {
    if (v) {
      flags |= SliderDataFlags::Stepped;
    } else {
      flags &= uint8_t(~SliderDataFlags::Stepped);
    }
  }

  bool is_dragging() const {
    return flags & SliderDataFlags::Dragging;
  }

  void set_dragging(bool v) {
    if (v) {
      flags |= SliderDataFlags::Dragging;
    } else {
      flags &= uint8_t(~SliderDataFlags::Dragging);
    }
  }

public:
  uint8_t flags;
  float value;
  float min_value;
  float max_value;
  float step_value;
  float coord0;
  float value0;
  float container_p0;
  float container_p1;
};

struct CheckboxData {
  bool checked;
};

struct Dropdown {
  bool empty() const {
    return box_item_end <= box_item_begin;
  }

  DropdownData* data;
  int box_item_begin;
  int box_item_end;
  DropdownCallback* select_callback;
};

struct Slider {
  SliderData* data;
  layout::BoxID box_handle;
  SliderDragCallback* drag_callback;
};

struct Checkbox {
  CheckboxData* data;
  layout::BoxID box_handle;
  CheckboxCallback* check_callback;
};

struct Button {
  layout::BoxID box_handle;
  ClickCallback* click_callback;
};

struct StatefulButton {
  layout::BoxID box_handle;
  StatefulButtonData data;
  StatefulClickCallback* click_callback;
};

struct Elements {
  Optional<int> layout_index;
  std::vector<layout::BoxID> dropdown_items;
  std::vector<Dropdown> dropdowns;
  std::vector<Slider> sliders;
  std::vector<Checkbox> checkboxes;
  std::vector<Button> buttons;
  std::vector<StatefulButton> stateful_buttons;
  bool began_dropdown{};
};

void begin_dropdown(Elements* elements, DropdownData* data, DropdownCallback* cb);
void push_dropdown_item(Elements* elements, int box);
void end_dropdown(Elements* elements);
void push_checkbox(Elements* elements, int box, CheckboxData* data, CheckboxCallback* cb);
void push_button(Elements* elements, int box, ClickCallback* cb);
void push_stateful_button(
  Elements* elements, int box, StatefulButtonData data, StatefulClickCallback* cb);
void push_slider(Elements* elements, int handle_box, SliderData* data, SliderDragCallback* cb);

void begin_elements(Elements* elements, int layout_index);
void evaluate(Elements* elements, const cursor::CursorState* cursor, void* callback_ptr);
void end_elements(Elements* elements);

}