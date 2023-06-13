#pragma once

#include "grove/math/vector.hpp"
#include <vector>

struct GLFWwindow;

namespace grove {

class IMGUIWrapper {
  struct WindowInfo {
    Vec2f p0{};
    Vec2f p1{};
  };

public:
  static constexpr const char* glsl_version = "#version 150";

public:
  IMGUIWrapper();
  ~IMGUIWrapper();

  void initialize(GLFWwindow* window);
  void new_frame();
  void render();
  bool cursor_intersects_with_window(double x, double y) const;

  void end_window();
  void new_null_frame();

private:
  void push_window_info(WindowInfo info);
  WindowInfo get_window_info() const;

private:
  bool is_initialized;
  std::vector<WindowInfo> window_info;
};

}