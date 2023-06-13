#pragma once

#include "grove/common/common.hpp"
#include <cstddef>
#include <memory>

namespace grove {
  template <typename T>
  struct Image;
}

template <typename T>
struct grove::Image {
  std::unique_ptr<T[]> data;
  int width;
  int height;
  int num_components_per_pixel;
  
  Image(std::unique_ptr<T[]> data, int width, int height, int num_components) :
    data(std::move(data)),
    width(width),
    height(height),
    num_components_per_pixel(num_components) {
    //
  }
  
  Image() : Image(nullptr, 0, 0, 0) {
    //
  }

  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(Image)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(Image)
  
  size_t size() const {
    return width * height * num_components_per_pixel;
  }

  int stride() const {
    return num_components_per_pixel;
  }

  T* ptr() {
    return data.get();
  }

  const T* ptr() const {
    return data.get();
  }
};
