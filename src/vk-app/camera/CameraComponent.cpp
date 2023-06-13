#include "CameraComponent.hpp"
#include "../terrain/terrain.hpp"
#include "../imgui/InputGUI.hpp"
#include "grove/input/Controller.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/ease.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

void control_camera(Camera& camera, const Controller& controller, float movement_speed,
                    bool ignore_rot_y, bool free_roam) {
  auto right = camera.get_right();
  right.y = 0.0f;
  right = normalize(right);

  auto front = free_roam ? -camera.get_front() : Vec3f{-right.z, 0.0f, right.x};

  Vec3f movement{};
  movement += right * float(controller.movement_x()) * movement_speed;
  movement += front * float(controller.movement_z()) * movement_speed;

  const float rot_y = ignore_rot_y ? 0.0f : float(controller.rotation_y());
  Vec3f rot(rot_y, float(controller.rotation_x()), 0.0f);
  camera.rotate(rot);
  camera.move(movement);
}

float get_height_edge(
  const CameraComponent::Params& params,
  CameraComponent::CameraPositionTarget target, float terrain_y) {
  //
  switch (target) {
    case CameraComponent::CameraPositionTarget::Ground:
      return terrain_y;
    case CameraComponent::CameraPositionTarget::HighUp:
      return params.high_up_height;
    case CameraComponent::CameraPositionTarget::BelowGround:
      return params.below_ground_height;
    default: {
      assert(false);
      return 0;
    }
  }
}

} //  anon

void CameraComponent::initialize(const InitInfo& info) {
  auto& camera = info.fps_camera;
  Camera::set_default_projection_info(camera, info.window_aspect_ratio);
  camera.set_position(Vec3f{0.0f, 5.5f, 0.0f});

//  camera.set_position(Vec3f{-80.325653f, 16.014454f, 78.973480f});
  camera.rotate(Vec3f{0.1f, 0.75f, 0.0f});
  //  app.camera.set_position(Vec3f{-31.325291f, 10.062595f, 15.544834f});
  //  app.camera.rotate(Vec3f{0.1f, pif() - 0.1f * pif(), 0.0f});
  //
  camera.update_view();
  camera.update_projection();
  params.move_speed = 0.25f;
}

CameraComponent::UpdateResult CameraComponent::update(const UpdateInfo& info) {
  UpdateResult result{};

  const bool ignore_rot_y = !params.free_roaming && target == CameraPositionTarget::HighUp;
  control_camera(
    info.fps_camera, info.controller, params.move_speed, ignore_rot_y, params.free_roaming);
  Camera::set_default_projection_info(info.fps_camera, info.window_aspect_ratio);

  Vec3f fps_pos = info.fps_camera.get_position();
  const float fps_terrain_h = info.terrain.height_nearest_position_xz(fps_pos);
  const float terrain_y = fps_terrain_h + params.fps_height;
  prev_terrain_height = terrain_y;

  if (!params.free_roaming) {
    const bool finished_transition = target_t == 1.0f;
    target_t = clamp01(target_t + float(info.real_dt * 1.25));

    const float h0 = get_height_edge(params, prev_target, terrain_y);
    const float h1 = get_height_edge(params, target, terrain_y);

    fps_pos.y = lerp(ease::in_out_expo(target_t), h0, h1);
    info.fps_camera.set_position(fps_pos);

    if (need_acquire_camera_theta0) {
      auto tmp_front = info.fps_camera.get_front();
      auto x = info.fps_camera.get_right();
      camera_theta0 = asin(tmp_front.y);
      camera_phi0 = atan2(x.x, x.z);
      need_acquire_camera_theta0 = false;
    }

    if (!finished_transition) {
      float target_theta = target == CameraPositionTarget::HighUp ? -pif() * 0.5f + 1e-1f : 0.0f;
      float curr_theta = lerp(ease::in_out_expo(target_t), camera_theta0, target_theta);
      const auto cp = std::cos(camera_phi0);
      const auto sp = std::sin(camera_phi0);
      const auto st = std::sin(curr_theta);
      const auto ct = std::cos(curr_theta);
      auto x = cp * ct;
      auto y = st;
      auto z = -sp * ct;
      Vec3f front{float(x), float(y), float(z)};
      info.fps_camera.set_front(front);
    }
  }

  info.fps_camera.update_view();
  info.fps_camera.update_projection();

  prev_height = info.fps_camera.get_position().y;

  result.is_below_ground = prev_height < fps_terrain_h;
  return result;
}

void CameraComponent::set_position_target(CameraPositionTarget targ) {
  if (targ != target) {
    prev_target = target;
    target = targ;
    need_acquire_camera_theta0 = true;

    float h0 = get_height_edge(params, prev_target, prev_terrain_height);
    float h1 = get_height_edge(params, target, prev_terrain_height);
    target_t = clamp01((prev_height - h0) / (h1 - h0));
  }
}

void CameraComponent::toggle_high_up_position_target() {
  if (target != CameraPositionTarget::HighUp) {
    set_position_target(CameraPositionTarget::HighUp);
  } else {
    set_position_target(CameraPositionTarget::Ground);
  }
}

void CameraComponent::toggle_below_ground_position_target() {
  if (target != CameraPositionTarget::BelowGround) {
    set_position_target(CameraPositionTarget::BelowGround);
  } else {
    set_position_target(CameraPositionTarget::Ground);
  }
}

void CameraComponent::on_gui_update(const InputGUIUpdateResult& res) {
  if (res.fps_camera_height) {
    params.fps_height = res.fps_camera_height.value();
  }
  if (res.move_speed) {
    params.move_speed = res.move_speed.value();
  }
}

GROVE_NAMESPACE_END
