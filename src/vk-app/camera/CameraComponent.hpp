#pragma once

#include "grove/common/Optional.hpp"

namespace grove {

class Controller;
class Camera;
class Terrain;

struct InputGUIUpdateResult;

class CameraComponent {
public:
  enum class CameraPositionTarget {
    Ground = 0,
    HighUp,
    BelowGround,
  };

  struct Params {
    float high_up_height{128.0f};
    float below_ground_height{-32.0f};
    float fps_height{5.5f};
    bool free_roaming{};
    float move_speed{0.2f};
  };

  struct InitInfo {
    Camera& fps_camera;
    float window_aspect_ratio{};
  };

  struct UpdateInfo {
    Camera& fps_camera;
    Controller& controller;
    float window_aspect_ratio{};
    const Terrain& terrain;
    double real_dt{};
  };

  struct UpdateResult {
    bool is_below_ground;
  };

public:
  void initialize(const InitInfo& info);
  UpdateResult update(const UpdateInfo& info);
  void set_position_target(CameraPositionTarget target);
  void toggle_high_up_position_target();
  void toggle_below_ground_position_target();
  void toggle_free_roaming() {
    params.free_roaming = !params.free_roaming;
  }
  void set_free_roaming(bool free) {
    params.free_roaming = free;
  }
  void on_gui_update(const InputGUIUpdateResult& res);
  const Params& get_params() const {
    return params;
  }

private:
  Params params{};
  CameraPositionTarget prev_target{CameraPositionTarget::HighUp};
  CameraPositionTarget target{};
  float prev_height{};
  float prev_terrain_height{};
  float target_t{1.0f};
  float camera_theta0{};
  float camera_phi0{};
  bool need_acquire_camera_theta0{};
};

}