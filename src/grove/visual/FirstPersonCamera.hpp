#pragma once

#include "Camera.hpp"

namespace grove {

class FirstPersonCamera : public Camera {
public:
  FirstPersonCamera();
  ~FirstPersonCamera() override = default;

  void move(const Vec3f& deltas) override;
  void rotate(const Vec3f& deltas) override;
  void set_projection_info(const ProjectionInfo& info) override;

  void set_position(const Vec3f& pos) override;

  void set_front(const Vec3f& v) override;
  Vec3f get_front() const override;
  Vec3f get_front_xz() const override;
  Vec3f get_right() const override;
  Vec3f get_position() const override;
  ProjectionInfo get_projection_info() const override;

  Mat4f get_projection() const override;
  Mat4f get_view() const override;

  void update_view() override;
  void update_projection() override;

private:
  Vec3f front;
  Vec3f position;

  ProjectionInfo projection_info;

  Mat4f view;
  Mat4f projection;
};

}