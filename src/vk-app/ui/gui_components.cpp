#include "gui_components.hpp"
#include "grove/math/util.hpp"
#include "grove/gui/gui_cursor.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gui;

void update_slider(elements::SliderData* data, const cursor::MouseState* state) {
  if (data->is_dragging()) {
    const float span_p = std::max(1e-3f, data->container_p1 - data->container_p0);
    const float span_v = data->max_value - data->min_value;
    const float delta11 = clamp((state->x - data->coord0) / span_p, -1.0f, 1.0f);
    data->value = clamp(data->value0 + span_v * delta11, data->min_value, data->max_value);
    if (data->is_stepped()) {
      assert(data->step_value > 0.0f);
      data->value = std::round(data->value / data->step_value) * data->step_value;
    }
  }
}

LabeledSliderBoxes layout_slider(
  layout::Layout* layout, int container, const layout::BoxDimensions& slider_w,
  const layout::BoxDimensions& slider_h, const layout::BoxDimensions& handle_w,
  const Optional<layout::BoxDimensions>& label_w, float f) {
  //
  assert(f >= 0.0f && f <= 1.0f);

  const float cont_w = layout::read_box(layout, container).content_width();
  float slider_px = slider_w.evaluate(cont_w);
  float rest_px{};

  if (label_w) {
    rest_px = std::min(label_w.value().evaluate(cont_w), std::max(0.0f, cont_w - slider_px));
  }

  layout::begin_group(layout, container, layout::GroupOrientation::Col, 0, 0, layout::JustifyContent::Left);
  int slider_section = layout::box(layout, slider_w, slider_h);
  int label_section{};
  if (label_w) {
    label_section = layout::box(layout, {1, rest_px, rest_px}, {1});
  }
  layout::end_group(layout);

  float handle_px = handle_w.evaluate(slider_px);
  float rem_dist = std::max(0.0f, slider_px - handle_px);
  float handle_off = rem_dist * f;

  layout::begin_manual_group(layout, slider_section);
  int handle = layout::box(layout, handle_w, {1});
  layout::set_box_offsets(layout, handle, handle_off, 0.0f);
  layout::set_box_cursor_events(layout, handle, {layout::BoxCursorEvents::Click});
  layout::end_group(layout);

  LabeledSliderBoxes result{};
  result.slider_section = slider_section;
  result.label_section = label_section;
  result.handle = handle;
  return result;
}

gui::DropdownBoxes layout_dropdown(
  layout::Layout* layout, int container, int clip_to_parent,
  const layout::BoxDimensions& w, const layout::BoxDimensions& h, bool open, int num_options) {
  //
  int off = layout::next_box_index(layout);
  if (open) {
    layout::begin_group(layout, container, layout::GroupOrientation::Row);
    for (int i = 0; i < num_options; i++) {
      int opt_box = layout::box(layout, w, h);
      layout::set_box_cursor_events(layout, opt_box, {layout::BoxCursorEvents::Click});
      layout::set_box_clip_to_parent_index(layout, opt_box, 0, clip_to_parent);
      layout::add_to_box_depth(layout, opt_box, 4);
    }
    layout::end_group(layout);
  } else {
    layout::begin_group(layout, container, layout::GroupOrientation::Row);
    int opt_box = layout::box(layout, w, h);
    layout::set_box_cursor_events(layout, opt_box, {layout::BoxCursorEvents::Click});
    layout::end_group(layout);
  }

  gui::DropdownBoxes result{};
  result.box_index_begin = off;
  result.box_index_end = layout::next_box_index(layout);
  return result;
}

} //  anon

int gui::prepare_row(layout::Layout* layout, const layout::BoxDimensions& h, float mt, float mb) {
  int row = layout::box(layout, {1}, h);
  layout::set_box_margin(layout, row, 0, mt, 0, mb);
  return row;
}

gui::SimpleSliderBoxes gui::prepare_simple_slider(
  elements::Elements& elements, elements::SliderData* slider_data,
  layout::Layout* layout, int container, const layout::BoxDimensions& slider_w,
  const layout::BoxDimensions& slider_h, const layout::BoxDimensions& handle_w,
  const cursor::CursorState* cursor_state, elements::SliderDragCallback* cb) {
  //
  update_slider(slider_data, gui::cursor::read_mouse_state(cursor_state));

  float min = slider_data->min_value;
  float max = slider_data->max_value;
  assert(min < max);
  const float v = clamp(slider_data->value, min, max);
  const float f = clamp01((v - min) / (max - min));

  auto res = layout_slider(layout, container, slider_w, slider_h, handle_w, NullOpt{}, f);

  auto slider_box = layout::read_box(layout, res.slider_section);
  auto handle_box = layout::read_box(layout, res.handle);

  slider_data->container_p0 = slider_box.content_x0;
  slider_data->container_p1 = slider_box.content_x1 - handle_box.content_width();
  elements::push_slider(&elements, res.handle, slider_data, cb);

  gui::SimpleSliderBoxes result{};
  result.handle = res.handle;
  result.slider_section = res.slider_section;
  return result;
}

gui::LabeledSliderBoxes gui::prepare_labeled_slider(
  elements::Elements& elements, elements::SliderData* slider_data,
  layout::Layout* layout, int container,
  const layout::BoxDimensions& slider_w, const layout::BoxDimensions& slider_h,
  const layout::BoxDimensions& handle_w, const layout::BoxDimensions& label_w,
  const cursor::CursorState* cursor_state, elements::SliderDragCallback* cb) {
  //
  update_slider(slider_data, gui::cursor::read_mouse_state(cursor_state));

  float min = slider_data->min_value;
  float max = slider_data->max_value;
  assert(min < max);
  const float v = clamp(slider_data->value, min, max);
  const float f = clamp01((v - min) / (max - min));

  auto opt_label_w = Optional<layout::BoxDimensions>(label_w);
  auto res = layout_slider(layout, container, slider_w, slider_h, handle_w, opt_label_w, f);

  auto slider_box = layout::read_box(layout, res.slider_section);
  auto handle_box = layout::read_box(layout, res.handle);

  slider_data->container_p0 = slider_box.content_x0;
  slider_data->container_p1 = slider_box.content_x1 - handle_box.content_width();
  elements::push_slider(&elements, res.handle, slider_data, cb);
  return res;
}

gui::DropdownBoxes gui::prepare_dropdown(
  elements::Elements& elements, elements::DropdownData* dropdown_data,
  layout::Layout* layout, int container, int clip,
  const layout::BoxDimensions& w, const layout::BoxDimensions& h,
  int num_options, elements::DropdownCallback* cb) {
  //
  elements::begin_dropdown(&elements, dropdown_data, cb);

  auto res = layout_dropdown(layout, container, clip, w, h, dropdown_data->open, num_options);
  for (int i = res.box_index_begin; i < res.box_index_end; i++) {
    elements::push_dropdown_item(&elements, i);
  }

  elements::end_dropdown(&elements);
  return res;
}

gui::LabeledCheckboxBoxes gui::prepare_labeled_checkbox(
  elements::Elements& elements, elements::CheckboxData* cb_data,
  layout::Layout* layout, int container,
  const layout::BoxDimensions& box_w, const layout::BoxDimensions& h,
  elements::CheckboxCallback* cb) {
  //
  const float w = layout::read_box(layout, container).content_width();
  const float cb_w = box_w.evaluate(w);
  const float label_w = std::max(1e-3f, w - cb_w);

  const auto just_left = layout::JustifyContent::Left;
  layout::begin_group(layout, container, layout::GroupOrientation::Col, 0, 0, just_left);
  int check_box = layout::box(layout, {1, cb_w, cb_w}, h);
  layout::set_box_cursor_events(layout, check_box, {layout::BoxCursorEvents::Click});
  int label = layout::box(layout, {1, label_w, label_w}, h);
  layout::end_group(layout);

  layout::begin_group(layout, check_box, layout::GroupOrientation::Row);
  int tick_box = layout::box(layout, {0.5f}, {0.5f});
  layout::set_box_cursor_events(layout, tick_box, {layout::BoxCursorEvents::Pass});
  layout::end_group(layout);

  elements::push_checkbox(&elements, check_box, cb_data, cb);

  gui::LabeledCheckboxBoxes result{};
  result.check_box = check_box;
  result.tick_box = tick_box;
  result.label_box = label;
  return result;
}

int gui::prepare_button(
  elements::Elements& elements, layout::Layout* layout,
  const layout::BoxDimensions& w, const layout::BoxDimensions& h,
  bool centered, elements::ClickCallback* cb) {
  //
  int button = layout::box(layout, w, h, centered);
  layout::set_box_cursor_events(layout, button, {layout::BoxCursorEvents::Click});
  elements::push_button(&elements, button, cb);
  return button;
}

GROVE_NAMESPACE_END
