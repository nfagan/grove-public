#include "gui_elements.hpp"
#include "gui_cursor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

layout::BoxID make_box_id(const elements::Elements* els, int box) {
  assert(els->layout_index);
  return layout::BoxID::create(els->layout_index.value(), box);
}

void clear_elements(elements::Elements* els) {
  els->dropdown_items.clear();
  els->dropdowns.clear();
  els->sliders.clear();
  els->checkboxes.clear();
  els->buttons.clear();
  els->stateful_buttons.clear();
}

void evaluate_buttons(
  elements::Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  for (auto& button : elements->buttons) {
    if (cursor::left_clicked_on(cursor, button.box_handle) && button.click_callback) {
      button.click_callback(callback_ptr);
    }
  }
}

void evaluate_stateful_buttons(
  elements::Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  for (auto& button : elements->stateful_buttons) {
    if (cursor::left_clicked_on(cursor, button.box_handle) && button.click_callback) {
      button.click_callback(callback_ptr, button.data);
    }
  }
}

void evaluate_checkboxes(
  elements::Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  for (auto& cb : elements->checkboxes) {
    if (cursor::left_clicked_on(cursor, cb.box_handle)) {
      cb.data->checked = !cb.data->checked;
      if (cb.check_callback) {
        cb.check_callback(cb.data->checked, callback_ptr);
      }
    }
  }
}

void evaluate_sliders(
  elements::Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  const auto& mouse = *gui::cursor::read_mouse_state(cursor);
  for (auto& s : elements->sliders) {
    if (!s.data->is_dragging() && cursor::newly_left_down_on(cursor, s.box_handle)) {
      s.data->set_dragging(true);
      s.data->coord0 = mouse.x;
      s.data->value0 = s.data->value;

    } else if (s.data->is_dragging()) {
      if (s.drag_callback) {
        s.drag_callback(s.data->value, callback_ptr);
      }

      if (!mouse.left_down) {
        s.data->set_dragging(false);
      }
    }
  }
}

void evaluate_dropdowns(
  elements::Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  const bool clicked = cursor::newly_left_clicked(cursor);
  int hit_dd{-1};

  for (auto& dd : elements->dropdowns) {
    if (dd.empty()) {
      continue;
    }
    assert(dd.box_item_begin < int(elements->dropdown_items.size()));
    if (!dd.data->open) {
      if (cursor::left_clicked_on(cursor, elements->dropdown_items[dd.box_item_begin])) {
        dd.data->open = true;
        hit_dd = int(&dd - elements->dropdowns.data());
      }
    } else {
      for (int i = dd.box_item_begin; i < dd.box_item_end; i++) {
        if (cursor::left_clicked_on(cursor, elements->dropdown_items[i])) {
          dd.data->option = i - dd.box_item_begin;
          dd.data->open = false;
          if (dd.select_callback) {
            dd.select_callback(dd.data->option, callback_ptr);
          }
          hit_dd = int(&dd - elements->dropdowns.data());
        }
      }
    }
  }

  if (clicked) {
    for (int i = 0; i < int(elements->dropdowns.size()); i++) {
      auto& dd = elements->dropdowns[i];
      if (dd.empty()) {
        continue;
      }
      if (dd.data->open && hit_dd != i) {
        dd.data->open = false;
      }
    }
  }
}

} //  anon

void gui::elements::begin_dropdown(Elements* elements, DropdownData* data, DropdownCallback* cb) {
  assert(!elements->began_dropdown);
  auto& dd = elements->dropdowns.emplace_back();
  dd.data = data;
  dd.select_callback = cb;
  dd.box_item_begin = int(elements->dropdown_items.size());
  elements->began_dropdown = true;
}

void gui::elements::push_dropdown_item(Elements* elements, int box) {
  assert(elements->began_dropdown);
  elements->dropdown_items.emplace_back() = make_box_id(elements, box);
}

void gui::elements::end_dropdown(grove::gui::elements::Elements* elements) {
  assert(elements->began_dropdown && !elements->dropdowns.empty());
  auto& dd = elements->dropdowns.back();
  dd.box_item_end = int(elements->dropdown_items.size());
  elements->began_dropdown = false;
}

void gui::elements::push_checkbox(
  Elements* elements, int box, CheckboxData* data, CheckboxCallback* cb) {
  //
  auto& el = elements->checkboxes.emplace_back();
  el.data = data;
  el.box_handle = make_box_id(elements, box);
  el.check_callback = cb;
}

void gui::elements::push_button(Elements* elements, int box, ClickCallback* cb) {
  auto& el = elements->buttons.emplace_back();
  el.box_handle = make_box_id(elements, box);
  el.click_callback = cb;
}

void gui::elements::push_stateful_button(
  Elements* elements, int box, StatefulButtonData data, StatefulClickCallback* cb) {
  //
  auto& el = elements->stateful_buttons.emplace_back();
  el.box_handle = make_box_id(elements, box);
  el.data = data;
  el.click_callback = cb;
}

void gui::elements::push_slider(Elements* elements, int handle_box, SliderData* data,
                                SliderDragCallback* cb) {
  auto& slide = elements->sliders.emplace_back();
  slide.data = data;
  slide.box_handle = make_box_id(elements, handle_box);
  slide.drag_callback = cb;
}

void gui::elements::begin_elements(Elements* elements, int layout_index) {
  assert(!elements->layout_index && layout_index > 0);
  elements->layout_index = layout_index;
  clear_elements(elements);
}

void gui::elements::evaluate(
  Elements* elements, const cursor::CursorState* cursor, void* callback_ptr) {
  //
  assert(elements->layout_index && "Call `begin_elements` before `evaluate`.");
  evaluate_buttons(elements, cursor, callback_ptr);
  evaluate_stateful_buttons(elements, cursor, callback_ptr);
  evaluate_checkboxes(elements, cursor, callback_ptr);
  evaluate_sliders(elements, cursor, callback_ptr);
  evaluate_dropdowns(elements, cursor, callback_ptr);
}

void gui::elements::end_elements(Elements* elements) {
  assert(elements->layout_index && "Call begin first.");
  elements->layout_index = NullOpt{};
}

GROVE_NAMESPACE_END
