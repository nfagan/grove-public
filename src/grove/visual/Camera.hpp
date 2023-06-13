#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"

namespace grove {

struct Frustum;

struct ProjectionInfo {
  ProjectionInfo();
  float projection_plane_distance() const;

  float near;
  float far;
  float aspect_ratio;
  float fov_y;
};

class Window;

class Camera {
public:
  virtual ~Camera() = default;

  Frustum make_world_space_frustum(float far = 0.0f) const;

  virtual void move(const Vec3f& deltas) = 0;
  virtual void rotate(const Vec3f& deltas) = 0;

  virtual void set_projection_info(const ProjectionInfo& info) = 0;
  virtual void set_position(const Vec3f& pos) = 0;

  virtual void set_front(const Vec3f& f) = 0;
  virtual Vec3f get_front() const = 0;
  virtual Vec3f get_front_xz() const = 0;
  virtual Vec3f get_right() const = 0;
  virtual Vec3f get_position() const = 0;
  Vec2f get_position_xz() const;
  virtual ProjectionInfo get_projection_info() const = 0;

  virtual Mat4f get_projection() const = 0;
  virtual Mat4f get_view() const = 0;

  virtual void update_view() = 0;
  virtual void update_projection() = 0;

  static Vec3f spherical_rotate(const Vec3f& front, float dtheta, float dphi,
                                float theta_min, float theta_max);

  static Mat4f projection_from_info(const ProjectionInfo& info);

  static void set_default_projection_info(Camera& camera, float aspect_ratio);
};

}