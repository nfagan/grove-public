#pragma once

#include "matrix.hpp"
#include "vector.hpp"
#include <string>

namespace grove {

namespace detail {
  template <typename T>
  struct VectorSize {
    static constexpr int size = 0;
  };

  template <typename T>
  struct VectorSize<Vec2<T>> {
    static constexpr int size = 2;
  };

  template <typename T>
  struct VectorSize<Vec3<T>> {
    static constexpr int size = 3;
  };

  template <typename T>
  struct VectorSize<Vec4<T>> {
    static constexpr int size = 4;
  };
}

template <typename T>
typename std::enable_if_t<detail::VectorSize<T>::size != 0, std::string> to_string(const T& v) {
  std::string res("(");
  for (int i = 0; i < detail::VectorSize<T>::size; i++) {
    res += std::to_string(v[i]);
    if (i < detail::VectorSize<T>::size-1) {
      res += ",";
    }
  }
  res += ")";
  return res;
}

template <typename T>
std::string to_string(const Mat4<T>& m) {
  std::string res("[");
  for (int i = 0; i < 4; i++) {
    res += to_string(m[i]);
    if (i < 3) {
      res += ",";
    }
  }
  res += "]";
  return res;
}

}