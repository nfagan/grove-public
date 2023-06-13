#include "Controller.hpp"
#include "grove/common/common.hpp"
#include "grove/math/vector.hpp"
#include "grove/visual/Camera.hpp"

GROVE_NAMESPACE_BEGIN

/*
 * Debug
 */

void Controller::debug_control_camera(const Controller& controller,
                                      Camera& camera,
                                      float movement_speed,
                                      bool constrain_xz) {
  auto right = camera.get_right();
  right.y = 0.0f;
  right = normalize(right) * float(controller.movement_x());

  auto front = constrain_xz ? camera.get_front_xz() : camera.get_front();

  Vec3f movement(0.0f);
  movement += right * movement_speed;
  movement += -front * float(controller.movement_z()) * movement_speed;

  Vec3f rot(float(controller.rotation_y()), float(controller.rotation_x()), 0.0f);
  camera.rotate(rot);
  camera.move(movement);
}

/*
 * VelocityHistory
 */

double Controller::VelocityHistory::update(double d, double dt, bool use_history) {
  history.push(d / dt);
  const double delta = use_history ? history.mean() : history.latest();
  return delta * dt;
}

GROVE_NAMESPACE_END
