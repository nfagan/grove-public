#include "KeyboardMouse.hpp"
#include "grove/math/util.hpp"

namespace grove {

namespace {

struct Config {
  static constexpr double min_rotation_time_constant = 1e-8;
  static constexpr double max_rotation_time_constant = 0.0025;

  static constexpr double min_mouse_sensitivity = 0.01 * 0.25;
  static constexpr double max_mouse_sensitivity = 0.1;
};

} //  anon

KeyboardMouseController::KeyboardMouseController(Keyboard* keyboard, Mouse* mouse) :
  keyboard(keyboard),
  mouse(mouse) {
  //
}

double KeyboardMouseController::get_rotation_sensitivity() const {
  double mn = Config::min_mouse_sensitivity;
  double mx = Config::max_mouse_sensitivity;
  return (clamp(mouse_sensitivity, mn, mx) - mn) / (mx - mn);
}

void KeyboardMouseController::set_rotation_sensitivity(double v) {
  v = clamp01(v);
  mouse_sensitivity = lerp(
    v, Config::min_mouse_sensitivity, Config::max_mouse_sensitivity);
}

double KeyboardMouseController::get_rotation_smoothing() const {
  double mn = Config::min_rotation_time_constant;
  double mx = Config::max_rotation_time_constant;
  return clamp01((clamp(rotation_time_constant, mn, mx) - mn) / (mx - mn));
}

void KeyboardMouseController::set_rotation_smoothing(double v) {
  v = clamp01(v);
  rotation_time_constant = lerp(
    v, Config::min_rotation_time_constant, Config::max_rotation_time_constant);
}

double KeyboardMouseController::movement_x() const {
  return direction.get_x() * movement_speed * movement_speed_scale;
}

double KeyboardMouseController::movement_z() const {
  return direction.get_z() * movement_speed * movement_speed_scale;
}

double KeyboardMouseController::rotation_x() const {
  return delta_x * mouse_sensitivity;
}

double KeyboardMouseController::rotation_y() const {
  return delta_y * mouse_sensitivity;
}

void KeyboardMouseController::set_mouse_sensitivity(double sens) {
  mouse_sensitivity = sens;
}

void KeyboardMouseController::update() {
  direction.clear();

  if (allow_movement) {
    if (keyboard->is_pressed(Key::S)) {
      direction.add_z(1.0);
    }
    if (keyboard->is_pressed(Key::W)) {
      direction.add_z(-1.0);
    }
    if (keyboard->is_pressed(Key::A)) {
      direction.add_x(-1.0);
    }
    if (keyboard->is_pressed(Key::D)) {
      direction.add_x(1.0);
    }
  }

  const auto current_coordinates = mouse->get_coordinates();

  if (require_shift_to_rotate) {
    if (keyboard->is_pressed(Key::LeftShift)) {
      target_x = current_coordinates.first;
      target_y = current_coordinates.second;
      if (!shift_pressed) {
        curr_x = target_x;
        curr_y = target_y;
        shift_pressed = true;
      }
    } else {
      shift_pressed = false;
    }
  } else {
    target_x = current_coordinates.first;
    target_y = current_coordinates.second;
  }

  const auto dt = stopwatch.delta_update().count();
  movement_speed_scale = dt / (1.0 / 60.0);

  const auto t = 1.0 - std::pow(rotation_time_constant, dt);
  auto last_x = curr_x;
  auto last_y = curr_y;
  curr_x = lerp(t, last_x, target_x);
  curr_y = lerp(t, last_y, target_y);

  delta_x = curr_x - last_x;
  delta_y = curr_y - last_y;
}

}
