#pragma once

#include "grove/common/Optional.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class CameraComponent;
class Camera;
class Controller;

struct InputGUIUpdateResult {
  Optional<float> fps_camera_height;
  Optional<float> move_speed;
  Optional<Vec3f> set_position;
  bool close{};
};

class InputGUI {
public:
  InputGUIUpdateResult render(
    CameraComponent& camera_component, Controller& controller, const Camera& camera);
};

}