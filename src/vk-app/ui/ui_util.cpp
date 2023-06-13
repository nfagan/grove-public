#include "ui_util.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/Window.hpp"
#include "grove/input/Mouse.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

Vec3f mouse_ray_direction(const Camera& camera, const Window& window, const Mouse& mouse) {
  auto inv_proj = inverse(camera.get_projection());
  auto inv_view = inverse(camera.get_view());

  auto coords = mouse.get_coordinates();
  Vec2f vec_coords(coords.first, coords.second);

  auto dims = window.dimensions();
  Vec2f vec_dims(float(dims.width), float(dims.height));

  return mouse_ray_direction(inv_view, inv_proj, vec_coords, vec_dims);
}

GROVE_NAMESPACE_END
