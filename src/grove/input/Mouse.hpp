#pragma once

#include <utility>

namespace grove {
  class Mouse;
}

class grove::Mouse {
public:
  enum class Button {
    Left = 0,
    Right,
    BUTTON_SIZE,
  };

  using Coordinates = std::pair<double, double>;

public:
  virtual ~Mouse() = default;
  
  virtual Coordinates get_coordinates() const = 0;
  virtual void set_coordinates(double x, double y) = 0;
  virtual void set_frame(float sx, float sy, float ox, float oy) = 0;

  virtual void mark_pressed(Button button) = 0;
  virtual void mark_released(Button button) = 0;

  virtual bool is_pressed(Button button) const = 0;

  static constexpr int number_of_buttons() {
    return int(Button::BUTTON_SIZE);
  }
};
