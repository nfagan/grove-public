#pragma once

#include "gui_layout.hpp"

namespace grove::gui::layout {

struct BoxSlot {
  float true_x1() const {
    return true_x0 + true_width;
  }
  float true_y1() const {
    return true_y0 + true_height;
  }

  int parent{};
  int child_box_offset{};
  int child_box_count{};
  uint16_t depth{};

  bool target_centered{};
  float target_width{};
  float target_min_width{};
  float target_max_width{};
  float target_height{};
  float target_min_height{};
  float target_max_height{};

  float pad_left{};
  float pad_top{};
  float pad_right{};
  float pad_bottom{};

  float margin_left{};
  float margin_top{};
  float margin_right{};
  float margin_bottom{};

  float true_x0{};
  float true_y0{};
  float scroll_x{};
  float scroll_y{};
  float true_width{};
  float true_height{};

  float clip_x0{};
  float clip_y0{};
  float clip_x1{};
  float clip_y1{};

  int16_t clip_to_parent_index_x{};
  int16_t clip_to_parent_index_y{};
  JustifyContent justify_content{};
  BoxCursorEvents events{};
};

}