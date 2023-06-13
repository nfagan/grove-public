#pragma once

#include "grove/gui/gui_layout.hpp"
#include "grove/gui/gui_elements.hpp"

namespace grove::gui {

struct LabeledSliderBoxes {
  int slider_section;
  int label_section;
  int handle;
};

struct SimpleSliderBoxes {
  int slider_section;
  int handle;
};

struct DropdownBoxes {
  int box_index_begin;
  int box_index_end;
};

struct LabeledCheckboxBoxes {
  int check_box;
  int tick_box;
  int label_box;
};

int prepare_row(layout::Layout* layout, const layout::BoxDimensions& h, float mt, float mb = 0.0f);

SimpleSliderBoxes prepare_simple_slider(
  elements::Elements& elements, elements::SliderData* slider_data,
  layout::Layout* layout, int container,
  const layout::BoxDimensions& slider_w, const layout::BoxDimensions& slider_h,
  const layout::BoxDimensions& handle_w, const cursor::CursorState* cursor_state,
  elements::SliderDragCallback* cb);

LabeledSliderBoxes prepare_labeled_slider(
  elements::Elements& elements, elements::SliderData* slider_data,
  layout::Layout* layout, int container,
  const layout::BoxDimensions& slider_w, const layout::BoxDimensions& slider_h,
  const layout::BoxDimensions& handle_w, const layout::BoxDimensions& label_w,
  const cursor::CursorState* cursor_state, elements::SliderDragCallback* cb);

DropdownBoxes prepare_dropdown(
  elements::Elements& elements, elements::DropdownData* dropdown_data,
  layout::Layout* layout, int container, int clip,
  const layout::BoxDimensions& w, const layout::BoxDimensions& h,
  int num_options, elements::DropdownCallback* cb);

LabeledCheckboxBoxes prepare_labeled_checkbox(
  elements::Elements& elements, elements::CheckboxData* cb_data,
  layout::Layout* layout, int container,
  const layout::BoxDimensions& box_w, const layout::BoxDimensions& h,
  elements::CheckboxCallback* cb);

int prepare_button(
  elements::Elements& elements, layout::Layout* layout,
  const layout::BoxDimensions& w, const layout::BoxDimensions& h,
  bool centered, elements::ClickCallback* cb);

}