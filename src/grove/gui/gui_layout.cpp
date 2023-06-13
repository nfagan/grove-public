#include "gui_layout.hpp"
#include "gui_layout_private.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"
#include <vector>
#include <cassert>

GROVE_NAMESPACE_BEGIN

namespace gui::layout {

struct Layout {
  uint8_t id{};
  std::vector<BoxSlot> box_slots;
  bool began{};
  GroupOrientation group_orientation{};
  int group_slot{};
};

} //  gui::layout

namespace {

gui::layout::ReadBox to_read_box(gui::layout::BoxID id, const gui::layout::BoxSlot& box) {
  gui::layout::ReadBox read{};
  read.id = id;
  read.events = box.events;
  read.depth = box.depth;
  read.x0 = box.true_x0;
  read.y0 = box.true_y0;
  read.x1 = box.true_x1();
  read.y1 = box.true_y1();

  read.content_x0 = box.true_x0 + box.pad_left;
  read.content_x1 = box.true_x1() - box.pad_right;
  read.content_y0 = box.true_y0 + box.pad_top;
  read.content_y1 = box.true_y1() - box.pad_bottom;

  read.clip_x0 = box.clip_x0;
  read.clip_y0 = box.clip_y0;
  read.clip_x1 = box.clip_x1;
  read.clip_y1 = box.clip_y1;
  return read;
}

int num_slots(const gui::layout::Layout* layout) {
  return int(layout->box_slots.size());
}

void clip_rect(float* x0, float* y0, float* x1, float* y1,
               const gui::layout::BoxSlot& parent_x,
               const gui::layout::BoxSlot& parent_y) {
  *x0 = clamp(*x0, parent_x.clip_x0, parent_x.clip_x1);
  *y0 = clamp(*y0, parent_y.clip_y0, parent_y.clip_y1);
  *x1 = clamp(*x1, parent_x.clip_x0, parent_x.clip_x1);
  *y1 = clamp(*y1, parent_y.clip_y0, parent_y.clip_y1);
}

float evaluate_dimension(const gui::layout::BoxDimensions& dim, float ref_value) {
  float x = dim.fraction * ref_value;
  if (dim.max >= 0.0f) {
    x = std::min(x, dim.max);
  }
  if (dim.min >= 0.0f) {
    x = std::max(x, dim.min);
  }
  return x;
}

float evaluate_width(const gui::layout::BoxSlot& box, float group_width) {
  float w = box.target_width * group_width;
  if (box.target_max_width >= 0.0f) {
    w = std::min(w, box.target_max_width);
  }
  if (box.target_min_width >= 0.0f) {
    w = std::max(w, box.target_min_width);
  }
  return w;
}

float evaluate_height(const gui::layout::BoxSlot& box, float group_height) {
  float h = box.target_height * group_height;
  if (box.target_max_height >= 0.0f) {
    h = std::min(h, box.target_max_height);
  }
  if (box.target_min_height >= 0.0f) {
    h = std::max(h, box.target_min_height);
  }
  return h;
}

const gui::layout::BoxSlot* get_clip_to_parent_index(
  const gui::layout::Layout* layout,
  const gui::layout::BoxSlot* parent, int ith) {
  //
  auto* clip_group = parent;
  for (int i = 0; i < ith; i++) {
    if (clip_group->parent >= 0) {
      clip_group = &layout->box_slots[clip_group->parent];
    } else {
      break;
    }
  }
  return clip_group;
}

} //  anon

gui::layout::Layout* gui::layout::create_layout(uint8_t id) {
  assert(id != 0);
  auto res = new gui::layout::Layout();
  res->id = id;
  clear_layout(res);
  return res;
}

void gui::layout::destroy_layout(Layout** layout) {
  delete *layout;
  *layout = nullptr;
}

void gui::layout::clear_layout(Layout* layout) {
  layout->box_slots.clear();
  BoxSlot root_slot{};
  root_slot.parent = -1;
  layout->box_slots.push_back(root_slot);
}

void gui::layout::set_root_dimensions(Layout* layout, float w, float h) {
  assert(!layout->box_slots.empty());
  auto& root = layout->box_slots[0];
  root.true_width = w;
  root.true_height = h;
  root.clip_x0 = root.true_x0;
  root.clip_x1 = root.true_x1();
  root.clip_y0 = root.true_y0;
  root.clip_y1 = root.true_y1();
}

uint8_t gui::layout::get_id(const Layout* layout) {
  return layout->id;
}

int gui::layout::total_num_boxes(const Layout* layout) {
  return num_slots(layout);
}

int gui::layout::read_boxes(const Layout* layout, ReadBox* dst, int n) {
  int res = std::min(n, total_num_boxes(layout));
  for (int i = 0; i < res; i++) {
    auto& box = layout->box_slots[i];
    dst[i] = to_read_box(BoxID::create(layout->id, i), box);
  }
  return res;
}

gui::layout::ReadBox gui::layout::read_box(const Layout* layout, int ith) {
  assert(ith >= 0 && ith < num_slots(layout));
  return to_read_box(BoxID::create(layout->id, ith), layout->box_slots[ith]);
}

void gui::layout::begin_group(Layout* layout, int box, GroupOrientation orientation,
                              float x_offset, float y_offset, JustifyContent justify_content,
                              const GroupPadding& pad) {
  assert(!layout->began);
  assert(box >= 0 && box < num_slots(layout));
  layout->began = true;
  layout->group_orientation = orientation;
  layout->group_slot = box;

  auto& par = layout->box_slots[box];
  par.child_box_offset = num_slots(layout);
  par.scroll_x = x_offset;
  par.scroll_y = y_offset;
  par.justify_content = justify_content;
  par.pad_left = pad.left;
  par.pad_top = pad.top;
  par.pad_right = pad.right;
  par.pad_bottom = pad.bottom;
}

void gui::layout::begin_manual_group(Layout* layout, int box) {
  begin_group(layout, box, layout::GroupOrientation::Manual, 0, 0, layout::JustifyContent::None);
}

int gui::layout::next_box_index(const Layout* layout) {
  return total_num_boxes(layout);
}

bool gui::layout::is_fully_clipped_box(const Layout* layout, int ith) {
  assert(ith >= 0 && ith < num_slots(layout));
  auto& box = layout->box_slots[ith];
  return box.clip_x1 <= box.clip_x0 || box.clip_y1 <= box.clip_y0;
}

int gui::layout::box(Layout* layout, const BoxDimensions& dim_x, const BoxDimensions& dim_y,
                     bool centered) {
  assert(layout->began);
  assert(dim_x.fraction >= 0.0f);
  assert(dim_y.fraction >= 0.0f);

  auto& par = layout->box_slots[layout->group_slot];
  par.child_box_count++;

  BoxSlot new_box{};
  new_box.parent = layout->group_slot;
  new_box.depth = par.depth + 1;
  new_box.target_width = dim_x.fraction;
  new_box.target_min_width = dim_x.min;
  new_box.target_max_width = dim_x.max;
  new_box.target_height = dim_y.fraction;
  new_box.target_min_height = dim_y.min;
  new_box.target_max_height = dim_y.max;
  new_box.target_centered = centered;

  auto res = num_slots(layout);
  layout->box_slots.push_back(new_box);

  return res;
}

void gui::layout::set_box_cursor_events(Layout* layout, int bi, BoxCursorEvents events) {
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  box.events = events;
}

void gui::layout::set_box_is_clickable(Layout* layout, int bi) {
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  box.events.bits |= BoxCursorEvents::Click;
}

void gui::layout::set_box_is_scrollable(Layout* layout, int bi) {
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  box.events.bits |= BoxCursorEvents::Scroll;
}

void gui::layout::set_box_margin(Layout* layout, int bi, float l, float t, float r, float b) {
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  box.margin_left = l;
  box.margin_top = t;
  box.margin_right = r;
  box.margin_bottom = b;
}

void gui::layout::set_box_clip_to_parent_index(Layout* layout, int bi, int ix, int iy) {
  assert(bi < num_slots(layout));
  assert(
    ix >= 0 && iy >= 0 &&
    ix < int(std::numeric_limits<int16_t>::max()) &&
    iy < int(std::numeric_limits<int16_t>::max()));
  auto& box = layout->box_slots[bi];
  box.clip_to_parent_index_x = int16_t(ix);
  box.clip_to_parent_index_y = int16_t(iy);
}

void gui::layout::set_box_offsets(Layout* layout, int bi, float x, float y) {
  assert(layout->began && layout->group_orientation == gui::layout::GroupOrientation::Manual);
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  box.true_x0 = x;
  box.true_y0 = y;
}

void gui::layout::add_to_box_depth(Layout* layout, int bi, int d) {
  assert(bi < num_slots(layout));
  auto& box = layout->box_slots[bi];
  assert(int(box.depth) + d >= 0);
  box.depth = uint16_t(int(box.depth) + d);
}

namespace {

void layout_group_manual(gui::layout::Layout* layout, const gui::layout::BoxSlot& group,
                         float group_width, float group_height, float* pxoff, float* pyoff) {
  float xoff = *pxoff;
  float yoff = *pyoff;

  for (int i = 0; i < group.child_box_count; i++) {
    int box_ind = i + group.child_box_offset;
    assert(box_ind < num_slots(layout));
    auto& box = layout->box_slots[box_ind];

    const float w = evaluate_width(box, group_width);
    const float h = evaluate_height(box, group_height);

    xoff += box.margin_left;
    yoff += box.margin_top;

    box.true_width = w;
    box.true_height = h;
    box.true_x0 += xoff;
    box.true_y0 += yoff;

    xoff += box.margin_right;
    yoff += box.margin_bottom;
  }

  *pxoff = xoff;
  *pyoff = yoff;
}

void layout_group_auto(gui::layout::Layout* layout, const gui::layout::BoxSlot& group,
                       float group_width, float group_height, float* pxoff, float* pyoff) {
  float xoff = *pxoff;
  float yoff = *pyoff;
  const float xoff0 = xoff;

  const bool is_col = layout->group_orientation == gui::layout::GroupOrientation::Col;
  const bool is_block = layout->group_orientation == gui::layout::GroupOrientation::Block;

  for (int i = 0; i < group.child_box_count; i++) {
    int box_ind = i + group.child_box_offset;
    assert(box_ind < num_slots(layout));
    auto& box = layout->box_slots[box_ind];

    const float w = evaluate_width(box, group_width);
    const float h = evaluate_height(box, group_height);

    if (!is_block && i == 0 && group.justify_content == gui::layout::JustifyContent::Right) {
      xoff -= w;
    }

    box.true_width = w;
    box.true_height = h;
    box.true_x0 = xoff;
    box.true_y0 = yoff;

    if (!is_block && box.target_centered) {
      if (is_col && h < group_height) {
        float off = (group_height - h) * 0.5f;
        box.true_y0 += off;
      } else if (!is_col && w < group_width) {
        float off = (group_width - w) * 0.5f;
        box.true_x0 += off;
      }
    }

    if (is_block && box.true_x1() > xoff0 + group_width) {
      box.true_x0 = xoff0 + box.margin_left;
      box.true_y0 = yoff + h + box.margin_bottom;
      yoff += h + box.margin_top + box.margin_bottom;
      xoff = xoff0 + w + box.margin_left + box.margin_right;

    } else {
      if (is_block || is_col) {
        float sign = group.justify_content == gui::layout::JustifyContent::Right ? -1.0f : 1.0f;
        xoff += sign * (w + box.margin_left + box.margin_right);
      } else {
        yoff += h + box.margin_top + box.margin_bottom;
      }
    }

    box.true_x0 += box.margin_left;
    box.true_y0 += box.margin_top;
  }

  *pxoff = xoff;
  *pyoff = yoff;
}

} //  anon

void gui::layout::end_group(Layout* layout) {
  assert(layout->began && layout->group_slot < int(layout->box_slots.size()));
  auto& group = layout->box_slots[layout->group_slot];
  layout->began = false;

  const bool is_col = layout->group_orientation == GroupOrientation::Col;
  const bool is_block = layout->group_orientation == GroupOrientation::Block;
  const bool is_manual = layout->group_orientation == GroupOrientation::Manual;
  assert(!is_block || (is_block && group.justify_content == JustifyContent::Left));
  assert(!is_manual || (is_manual && group.justify_content == JustifyContent::None));

  float yoff = group.true_y0 + group.pad_top;
  float xoff = group.true_x0 + group.pad_left;

  if (group.justify_content == JustifyContent::Right) {
    xoff = group.true_x1() - group.pad_right;
  }

  float group_width{};
  if (group.true_width > 0.0f) {
    group_width = std::max(1e-3f, group.true_width - (group.pad_right + group.pad_left));
  }

  float group_height{};
  if (group.true_height > 0.0f) {
    group_height = std::max(1e-3f, group.true_height - (group.pad_bottom + group.pad_top));
  }

  if (is_manual) {
    layout_group_manual(layout, group, group_width, group_height, &xoff, &yoff);
  } else {
    layout_group_auto(layout, group, group_width, group_height, &xoff, &yoff);
  }

  if (!is_block && group.justify_content == JustifyContent::Center && group.child_box_count > 0) {
    float rem;
    if (is_col) {
      rem = std::max(0.0f, group_width - (xoff - (group.true_x0 + group.pad_left)));
    } else {
      rem = std::max(0.0f, group_height - (yoff - (group.true_y0 + group.pad_top)));
    }

    if (rem > 0.0f) {
      float between = rem / float(group.child_box_count + 1);
      for (int i = 0; i < group.child_box_count; i++) {
        auto& box = layout->box_slots[i + group.child_box_offset];
        if (is_col) {
          box.true_x0 += between * float(i + 1);
        } else {
          box.true_y0 += between * float(i + 1);
        }
      }
    }
  }

  for (int i = 0; i < group.child_box_count; i++) {
    auto& box = layout->box_slots[group.child_box_offset + i];
    box.true_x0 += group.scroll_x;
    box.true_y0 += group.scroll_y;
  }

  for (int i = 0; i < group.child_box_count; i++) {
    auto& box = layout->box_slots[group.child_box_offset + i];
    box.clip_x0 = box.true_x0;
    box.clip_x1 = box.true_x1();
    box.clip_y0 = box.true_y0;
    box.clip_y1 = box.true_y1();

    auto* clip_group_x = get_clip_to_parent_index(layout, &group, box.clip_to_parent_index_x);
    auto* clip_group_y = get_clip_to_parent_index(layout, &group, box.clip_to_parent_index_y);
    clip_rect(&box.clip_x0, &box.clip_y0, &box.clip_x1, &box.clip_y1, *clip_group_x, *clip_group_y);
  }
}

const gui::layout::BoxSlot* gui::layout::read_box_slot_begin(const Layout* layout) {
  return layout->box_slots.data();
}

gui::layout::ReadBox gui::layout::evaluate_clipped_box_centered(
  const Layout* layout, int src, const BoxDimensions& w, const BoxDimensions& h) {
  assert(src >= 0 && src < num_slots(layout));

  auto& src_box = layout->box_slots[src];
  gui::layout::ReadBox result{};

  float rw = evaluate_dimension(w, src_box.true_width);
  float rh = evaluate_dimension(h, src_box.true_height);

  result.x0 = src_box.true_x0 + src_box.true_width * 0.5f - rw * 0.5f;
  result.y0 = src_box.true_y0 + src_box.true_height * 0.5f - rh * 0.5f;
  result.x1 = result.x0 + rw;
  result.y1 = result.y0 + rh;

  result.clip_x0 = result.x0;
  result.clip_y0 = result.y0;
  result.clip_x1 = result.x1;
  result.clip_y1 = result.y1;

  result.content_x0 = result.x0;
  result.content_y0 = result.y0;
  result.content_x1 = result.x1;
  result.content_y1 = result.y1;

  if (src_box.parent >= 0) {
    auto& par_box = layout->box_slots[src_box.parent];
    clip_rect(&result.clip_x0, &result.clip_y0, &result.clip_x1, &result.clip_y1, par_box, par_box);
  }

  return result;
}

void gui::layout::ReadBox::as_clipping_rect(float* px0, float* py0, float* px1, float* py1) const {
  *px0 = clamp(*px0, clip_x0, clip_x1);
  *py0 = clamp(*py0, clip_y0, clip_y1);
  *px1 = clamp(*px1, clip_x0, clip_x1);
  *py1 = clamp(*py1, clip_y0, clip_y1);
}

float gui::layout::BoxDimensions::evaluate(float x) const {
  return evaluate_dimension(*this, x);
}

GROVE_NAMESPACE_END
