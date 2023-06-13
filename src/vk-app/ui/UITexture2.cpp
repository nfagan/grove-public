#include "UITexture2.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

UITexture2::UITexture2(int texture_dim, int num_channels) :
  texture_dimension{texture_dim},
  num_channels{num_channels},
  texture_data(new uint8_t[texture_dim * texture_dim * num_channels]{}) {
  //
}

int UITexture2::texture_dim() const {
  return texture_dimension;
}

int UITexture2::data_size() const {
  auto tex_dim = texture_dim();
  return tex_dim * tex_dim * num_channels;
}

void UITexture2::clear() {
  auto* begin = texture_data.get();
  auto* end = begin + data_size();
  std::fill(begin, end, uint8_t(0));
}

namespace {
  inline int frac_to_pixel(float v, int dim) {
    return int(v * float(dim));
  }
}

void UITexture2::fill_frac_rect(const Vec2f& frac_center,
                                const Vec2f& frac_dims,
                                const Vec3f& color) {
  auto cx = frac_to_pixel(frac_center.x, texture_dim());
  auto cy = frac_to_pixel(frac_center.y, texture_dim());
  auto w = frac_to_pixel(frac_dims.x, texture_dim());
  auto h = frac_to_pixel(frac_dims.y, texture_dim());

  w = std::max(w/2, 1);
  h = std::max(h/2, 1);

  int min_col = cx - w;
  int max_col = cx + w;
  int min_row = cy - h;
  int max_row = cy + h;

  fill_rect(min_row, max_row, min_col, max_col, color);
}

void UITexture2::fill_rect(int min_row, int max_row,
                           int min_col, int max_col,
                           const Vec3f& color) {
  const auto dim = texture_dim();
  auto* data = texture_data.get();

  for (int i = min_row; i <= max_row; i++) {
    for (int j = min_col; j <= max_col; j++) {
      if (i < 0 || j < 0 || i >= dim || j >= dim) {
        continue;
      }

      auto px = j * dim + i;
      assert(px >= 0 && px < dim * dim);

      for (int k = 0; k < 3; k++) {
        data[px * num_channels + k] = uint8_t(color[k] * 0xff);
      }
    }
  }
}

GROVE_NAMESPACE_END
