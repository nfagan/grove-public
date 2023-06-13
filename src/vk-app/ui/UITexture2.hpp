#pragma once

#include "grove/math/vector.hpp"
#include <memory>

namespace grove {

class UITexture2 {
public:
  UITexture2(int texture_dim, int num_channels);
  void clear();
  void fill_rect(int min_row, int max_row,
                 int min_col, int max_col,
                 const Vec3f& color);
  void fill_frac_rect(const Vec2f& frac_center,
                      const Vec2f& frac_dims,
                      const Vec3f& color);
  int texture_dim() const;
  const uint8_t* read_data() const {
    return texture_data.get();
  }
private:
  int data_size() const;
private:
  int texture_dimension{};
  int num_channels{};
  std::unique_ptr<uint8_t[]> texture_data;
};

}