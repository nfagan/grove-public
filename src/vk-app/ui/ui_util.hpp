#pragma once

#include "grove/math/vector.hpp"
#include "grove/math/intersect.hpp"

namespace grove {

class Mouse;
class Window;
class Camera;

Vec3f mouse_ray_direction(const Camera& camera, const Window& window, const Mouse& mouse);

struct MouseState {
  bool left_clicked = false;
  bool right_clicked = false;
  Vec2f coordinates{};
};

inline bool point_rect_intersect(const Vec2f& query_point, const Vec2f& center, const Vec2f& size) {
  auto half_sz = size * 0.5f;
  auto p0 = center - half_sz;
  auto p1 = center + half_sz;
  return point_aabb_intersect(query_point, p0, p1);
}

namespace colors {
  constexpr Vec3f midi_message = Vec3f(1.0f, 0.5f, 0.0f);
  constexpr Vec3f midi_note = Vec3f(1.0f, 1.0f, 0.25f);
  constexpr Vec3f midi_instrument_input_output = Vec3f(1.0f, 0.25f, 1.0f);
  constexpr Vec3f float_data = Vec3f(0.25f);
  constexpr Vec3f int_data = Vec3f(0.25f, 0.0f, 0.0f);
  constexpr Vec3f sample2 = Vec3f(1.0f, 0.0f, 0.0f);
  constexpr Vec3f white = Vec3f(1.0f);
  constexpr Vec3f black = Vec3f(0.0f);
  constexpr Vec3f yellow = Vec3f(1.0f, 1.0f, 0.0f);
  constexpr Vec3f red = Vec3f(1.0f, 0.0f, 0.0f);
  constexpr Vec3f green = Vec3f(0.0f, 1.0f, 0.0f);
  constexpr Vec3f blue = Vec3f(0.0f, 0.0f, 1.0f);
  constexpr Vec3f mid_gray = Vec3f(0.5f);
}

}