#pragma once

#include "../Controller.hpp"
#include "../Keyboard.hpp"
#include "../Mouse.hpp"
#include "../Directional.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

class KeyboardMouseController : public Controller {
public:
  KeyboardMouseController(Keyboard* keyboard, Mouse* mouse);
  ~KeyboardMouseController() override = default;

  double get_rotation_sensitivity() const override;
  void set_rotation_sensitivity(double v) override;

  double get_rotation_smoothing() const override;
  void set_rotation_smoothing(double v) override;

  double movement_x() const override;
  double movement_z() const override;
  double rotation_x() const override;
  double rotation_y() const override;
  void clear_rotation_x() override {
    delta_x = 0.0;
  }
  void clear_rotation_y() override {
    delta_y = 0.0;
  }

  void update() override;
  void set_mouse_sensitivity(double sens);

public:
  bool require_shift_to_rotate{true};
  bool allow_movement{true};

private:
  Stopwatch stopwatch;

  bool shift_pressed{};
  double target_x{};
  double target_y{};
  double curr_x{};
  double curr_y{};
  double delta_x{};
  double delta_y{};
  double mouse_sensitivity{0.01};
  double movement_speed{2.0};
  double movement_speed_scale{1.0};
  double rotation_time_constant{0.0025};

  Keyboard* keyboard;
  Mouse* mouse;
  input::Directional direction;
};

}