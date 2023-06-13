#pragma once

#include "grove/common/History.hpp"
#include <array>

namespace grove {

class Camera;
class Controller;

class Controller {
protected:
  class VelocityHistory {
  public:
    double update(double v, double dt, bool use_history);

  private:
    History<double, 10> history{};
  };

public:
  virtual ~Controller() = default;

  virtual double movement_x() const = 0;
  virtual double movement_z() const = 0;

  virtual double get_rotation_sensitivity() const = 0; //  0-1
  virtual void set_rotation_sensitivity(double s) = 0;

  virtual double get_rotation_smoothing() const = 0; //  0-1
  virtual void set_rotation_smoothing(double s) = 0;

  virtual double rotation_x() const = 0;
  virtual double rotation_y() const = 0;

  virtual void clear_rotation_x() = 0;
  virtual void clear_rotation_y() = 0;
  virtual void update() = 0;

  static void debug_control_camera(const Controller& controller,
                                   Camera& camera, float movement_speed,
                                   bool constrain_xz = false);
};

}