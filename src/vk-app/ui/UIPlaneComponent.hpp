#pragma once

#include "UIPlaneCloth.hpp"
#include "UITexture2.hpp"
#include "UIPlane.hpp"
#include "grove/math/Ray.hpp"
#include "grove/math/Bounds2.hpp"

namespace grove {

class UIPlaneComponent {
public:
  static constexpr int ui_texture_dim = 512;
  static constexpr int num_ui_texture_channels = 4;

public:
  struct UpdateInfo {
    const Ray& mouse_ray;
    float height_at_plane_origin;
    const Vec2<double>& mouse_coordinates;
    const Vec2<double>& window_dimensions;
  };

  struct UpdateResult {
    UIPlane::HitInfo ui_plane_hit_info;
  };

public:
  UIPlaneComponent();
  UpdateResult begin_update(const UpdateInfo& info);
  UITexture2& get_ui_texture() {
    return ui_texture;
  }
  Vec3f get_ui_plane_center() const;
  Bounds2f get_ui_plane_world_bound_xz() const;

private:
  UITexture2 ui_texture;
  UIPlane ui_plane;
  UIPlaneCloth ui_plane_cloth;
  float height_at_plane_origin{};
};

}