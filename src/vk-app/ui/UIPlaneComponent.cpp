#include "UIPlaneComponent.hpp"
#include "grove/common/common.hpp"

#define UI_PLANE_IN_WORLD_SPACE (0)

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] UIPlane::HitInfo screen_space_ui_plane_hit_info(Vec2<double> mouse_pos,
                                                                 Vec2<double> window_dims) {
  auto f = (mouse_pos + 0.5) / window_dims;
  UIPlane::HitInfo hit{};
  hit.frac_hit_point = Vec2f{float(f.x), float(1.0 - f.y)};
  hit.hit = true;
  return hit;
}

} //  anon

UIPlaneComponent::UIPlaneComponent() :
  ui_texture{ui_texture_dim, num_ui_texture_channels} {
  //
}

UIPlaneComponent::UpdateResult UIPlaneComponent::begin_update(const UpdateInfo& info) {
  UpdateResult result{};

#if UI_PLANE_IN_WORLD_SPACE
  ui_plane_cloth.update(0.0f);
#endif
  ui_texture.clear();

  if (info.height_at_plane_origin != height_at_plane_origin) {
    height_at_plane_origin = info.height_at_plane_origin;
    ui_plane_cloth.on_new_height_map(height_at_plane_origin);
  }

  auto pos_data = ui_plane_cloth.get_position_data(height_at_plane_origin);
  const Bounds3f plane_bounds{pos_data.bounds_p0, pos_data.bounds_p1};
  ui_plane.update(info.mouse_ray, pos_data.plane, plane_bounds);
#if UI_PLANE_IN_WORLD_SPACE
  result.ui_plane_hit_info = ui_plane.get_mouse_hit_info();
#else
  result.ui_plane_hit_info = screen_space_ui_plane_hit_info(
    info.mouse_coordinates, info.window_dimensions);
#endif
  return result;
}

Vec3f UIPlaneComponent::get_ui_plane_center() const {
  auto pos_data = ui_plane_cloth.get_position_data(height_at_plane_origin);
  return pos_data.bounds_p0 + (pos_data.bounds_p1 - pos_data.bounds_p0) * 0.5f;
}

Bounds2f UIPlaneComponent::get_ui_plane_world_bound_xz() const {
  auto pos_data = ui_plane_cloth.get_position_data(height_at_plane_origin);
  return Bounds2f{
    Vec2f{pos_data.bounds_p0.x, pos_data.bounds_p0.z},
    Vec2f{pos_data.bounds_p1.x, pos_data.bounds_p1.z},
  };
}

GROVE_NAMESPACE_END
