#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

namespace grove::gui::layout {

struct BoxSlot;

enum class GroupOrientation : uint8_t {
  Col,
  Row,
  Block,
  Manual
};

enum class JustifyContent : uint8_t {
  Center,
  Left,
  Right,
  None
};

struct GroupPadding {
  float left;
  float top;
  float right;
  float bottom;
};

struct BoxDimensions {
  float evaluate(float x) const;

  float fraction{};
  float min{-1.0f};
  float max{-1.0f};
};

struct BoxCursorEvents {
public:
  static constexpr uint8_t Pass = 1u;
  static constexpr uint8_t Click = 2u;
  static constexpr uint8_t Scroll = 4u;

public:
  bool pass() const {
    return bits & Pass;
  }
  bool click() const {
    return bits & Click;
  }
  bool scroll() const {
    return bits & Scroll;
  }

public:
  uint8_t bits;
};

struct BoxID {
  struct Hash {
    std::size_t operator()(const BoxID& id) const noexcept {
      return id.layout_and_box_index;
    }
  };

  int index() const {
    return int(((layout_and_box_index >> 8u) & 0xffffffu));
  }

  static BoxID create(int layout_ind, int box_ind) {
    assert(layout_ind >= 0 && box_ind >= 0);
    assert(layout_ind < int(1u << 8u) && box_ind < int(1u << 24u));
    return BoxID{uint32_t(layout_ind) | (uint32_t(box_ind) << 8u)};
  }
  friend inline bool operator==(const BoxID& a, const BoxID& b) {
    return a.layout_and_box_index == b.layout_and_box_index;
  }
  friend inline bool operator!=(const BoxID& a, const BoxID& b) {
    return !(a == b);
  }

  uint32_t layout_and_box_index;
};

struct ReadBox {
  bool is_clipped() const {
    return clip_x1 <= clip_x0 || clip_y1 <= clip_y0;
  }
  float content_width() const {
    return content_x1 - content_x0;
  }
  float content_height() const {
    return content_y1 - content_y0;
  }
  void as_clipping_rect(float* px0, float* py0, float* px1, float* py1) const;
  void set_clipping_rect_from_full_rect() {
    clip_x0 = x0;
    clip_y0 = y0;
    clip_x1 = x1;
    clip_y1 = y1;
  }

  BoxID id;
  BoxCursorEvents events;
  float x0;
  float y0;
  float x1;
  float y1;
  float content_x0;
  float content_y0;
  float content_x1;
  float content_y1;
  float clip_x0;
  float clip_x1;
  float clip_y0;
  float clip_y1;
  uint16_t depth;
};

struct Layout;
Layout* create_layout(uint8_t id);
void destroy_layout(Layout** layout);
void clear_layout(Layout* layout);
void set_root_dimensions(Layout* layout, float w, float h);
uint8_t get_id(const Layout* layout);

void begin_group(Layout* layout, int box, GroupOrientation orientation,
                 float x_offset = 0.0f, float y_offset = 0.0f,
                 JustifyContent justify = JustifyContent::Center, const GroupPadding& pad = {});
void begin_manual_group(Layout* layout, int box);
int next_box_index(const Layout* layout);
int box(Layout* layout, const BoxDimensions& dim_x, const BoxDimensions& dim_y, bool centered = true);
void set_box_margin(Layout* layout, int box, float l, float t, float r, float b);
void set_box_cursor_events(Layout* layout, int box, BoxCursorEvents events);
void set_box_is_clickable(Layout* layout, int box);
void set_box_is_scrollable(Layout* layout, int box);
void set_box_clip_to_parent_index(Layout* layout, int box, int ix, int iy);
void set_box_offsets(Layout* layout, int box, float x, float y);
void add_to_box_depth(Layout* layout, int box, int d);
void end_group(Layout* layout);

int total_num_boxes(const Layout* layout);
int read_boxes(const Layout* layout, ReadBox* dst, int n);
ReadBox read_box(const Layout* layout, int ith);
const BoxSlot* read_box_slot_begin(const Layout* layout);
bool is_fully_clipped_box(const Layout* layout, int ith);

ReadBox evaluate_clipped_box_centered(
  const Layout* layout, int src, const BoxDimensions& w, const BoxDimensions& h);

}