#pragma once

#include "grove/math/vector.hpp"
#include <cstdint>

namespace grove::brush {

using ImageShape = Vec2<int64_t>;

template <typename T, typename U>
void fill(T&& dest, const ImageShape& shape, const Vec2<double>& center, int64_t radius, U&& op) {
  const auto px_center = center * Vec2<double>{double(shape.x), double(shape.y)};

  const auto center_x = int64_t(px_center.x);
  const auto center_y = int64_t(px_center.y);

  for (int64_t i = 0; i < radius * 2; i++) {
    for (int64_t j = 0; j < radius * 2; j++) {
      auto px_x = center_x + (i - radius);
      auto px_y = center_y + (j - radius);

      if (px_x >= shape.x || px_y >= shape.y || px_x < 0 || px_y < 0) {
        continue;
      }

      const Vec2<double> pixel{double(px_x), double(px_y)};
      const auto px_dir = pixel - px_center;

      const auto dist_frac = px_dir.length() / double(radius);
      if (dist_frac >= 1.0) {
        continue;
      }

      const auto px_index = px_x * shape.y + px_y;
      op(dest, px_index, px_dir);
    }
  }
}

}