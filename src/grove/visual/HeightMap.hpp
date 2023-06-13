#pragma once

#include "Image.hpp"
#include "grove/common/common.hpp"
#include "grove/common/config.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/util.hpp"
#include <algorithm>
#include <cmath>

GROVE_NAMESPACE_BEGIN

template <typename T, typename U = Image<T>>
class HeightMap {
public:
  struct BorrowedData {
    BorrowedData();
    BorrowedData(T* data, int width, int height, int num_components);

    int size() const;
    int stride() const;
    const T* ptr() const;

    T* data;
    int width;
    int height;
    int num_components;
  };

public:
  HeightMap();
  explicit HeightMap(U in_source);

  double normalized_value_at_normalized_xz(double x, double z, int component_index = 0) const;
  double raw_value_at_normalized_xz(double x, double z, int component_index = 0) const;
  void set_interpolation_extent(double extent);
  double get_interpolation_extent() const;

private:
  void establish_min_max_values();
  double get_pixel_value(int x_pixel, int z_pixel, int component_index) const;
  double interpolate(int x_pixel, int z_pixel, int component_index) const;

private:
  U source;

  double interpolation_extent;

  T min_value;
  T max_value;
};

/*
 * Impl
 */

/*
 * BorrowedData
 */

template <typename T, typename U>
HeightMap<T, U>::BorrowedData::BorrowedData() :
  data(nullptr),
  width(0),
  height(0),
  num_components(0) {
  //
}

template <typename T, typename U>
HeightMap<T, U>::BorrowedData::BorrowedData(T* data, int width, int height, int num_components) :
  data(data),
  width(width),
  height(height),
  num_components(num_components) {
  //
}

template <typename T, typename U>
int HeightMap<T, U>::BorrowedData::size() const {
  return width * height * num_components;
}

template <typename T, typename U>
int HeightMap<T, U>::BorrowedData::stride() const {
  return num_components;
}

template <typename T, typename U>
const T* HeightMap<T, U>::BorrowedData::ptr() const {
  return data;
}

/*
 * HeightMap
 */

template <typename T, typename U>
HeightMap<T, U>::HeightMap() :
  interpolation_extent(0.0),
  min_value(0),
  max_value(0) {
  //
}

template <typename T, typename U>
HeightMap<T, U>::HeightMap(U in_source) :
  source(std::move(in_source)),
  interpolation_extent(0.0) {
  //
  establish_min_max_values();
}

template <typename T, typename U>
double HeightMap<T, U>::raw_value_at_normalized_xz(double x, double z, int component_index) const {
  if (source.width == 0 || source.height == 0) {
    return 0.0;
  }

  x = grove::clamp(x, 0.0, 1.0);
  z = grove::clamp(z, 0.0, 1.0);

  if (!std::isfinite(x) || !std::isfinite(z)) {
    GROVE_LOG_WARNING("X or Z were NaN or non-finite.");
    return 0.0;
  }

  int x_pixel = int(x * double(source.width-1));
  int z_pixel = int(z * double(source.height-1));

  if (interpolation_extent == 0.0) {
    return get_pixel_value(x_pixel, z_pixel, component_index);
  } else {
    return interpolate(x_pixel, z_pixel, component_index);
  }
}

template <typename T, typename U>
double HeightMap<T, U>::normalized_value_at_normalized_xz(double x, double z,
                                                       int component_index) const {

  const double value = raw_value_at_normalized_xz(x, z, component_index);
  const double result = (value - min_value) / (max_value - min_value);

  return std::isfinite(result) ? result : 0.0;
}

template <typename T, typename U>
double HeightMap<T, U>::get_interpolation_extent() const {
  return interpolation_extent;
}

template <typename T, typename U>
void HeightMap<T, U>::set_interpolation_extent(double extent) {
  interpolation_extent = grove::clamp(extent, 0.0, 1.0);
}

template <typename T, typename U>
void HeightMap<T, U>::establish_min_max_values() {
  if (source.size() > 0) {
    min_value = *std::min_element(source.ptr(), source.ptr() + source.size());
    max_value = *std::max_element(source.ptr(), source.ptr() + source.size());
  } else {
    min_value = T(0);
    max_value = T(0);
  }
}

template <typename T, typename U>
double HeightMap<T, U>::get_pixel_value(int x_pixel, int z_pixel, int component_index) const {
  if (source.width == 0 || source.height == 0) {
    return 0.0;

  } else {
    const int z_index = z_pixel * source.stride() * source.width;
    const int x_index = x_pixel * source.stride() + component_index;
    const int index = z_index + x_index;

#ifdef GROVE_DEBUG
    if (index < 0 || index >= source.size()) {
      GROVE_LOG_ERROR("HeightMap: get_pixel_value: Out of bounds read.");
      return 0.0;
    }
#endif

    return double(source.data[index]);
  }
}

template <typename T, typename U>
double HeightMap<T, U>::interpolate(int x_pixel, int z_pixel, int component_index) const {
  if (source.width == 0 || source.height == 0) {
    return 0.0;
  }

  const int interp_x = int(double(source.width-1) * interpolation_extent);
  const int interp_z = int(double(source.height-1) * interpolation_extent);

  const int min_pixel_x = grove::clamp(x_pixel - interp_x/2, 0, source.width-1);
  const int max_pixel_x = grove::clamp(min_pixel_x + interp_x, 0, source.width-1);

  const int min_pixel_z = grove::clamp(z_pixel - interp_z/2, 0, source.height-1);
  const int max_pixel_z = grove::clamp(min_pixel_z + interp_z, 0, source.height-1);

  double actual_value = get_pixel_value(x_pixel, z_pixel, component_index);
  double iters = 1.0;

  for (int i = min_pixel_x; i < max_pixel_x; i++) {
    for (int j = min_pixel_z; j < max_pixel_z; j++) {
      if (i != x_pixel && j != z_pixel) {
        const T nearby = T(get_pixel_value(i, j, component_index));
        actual_value = (actual_value * iters + nearby) / (iters + 1.0);
        iters++;
      }
    }
  }

  return actual_value;
}

GROVE_NAMESPACE_END

