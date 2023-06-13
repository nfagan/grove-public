#pragma once

#include "Vec2.hpp"
#include <cstring>

namespace grove {

template <typename T>
struct Mat2 {
  constexpr Mat2() = default;

  constexpr explicit Mat2(T diag);
  constexpr explicit Mat2(const Vec2<T>& a, const Vec2<T>& b);
  constexpr explicit Mat2(T m00, T m01,
                          T m10, T m11);
  const T& operator()(int r, int c) const;
  T& operator()(int r, int c);

public:
  T elements[4];
};

template <typename T>
constexpr Mat2<T>::Mat2(T diag):
  elements{diag, T(0), T(0), diag} {
  //
}

template <typename T>
constexpr Mat2<T>::Mat2(const Vec2<T>& a, const Vec2<T>& b) :
  elements{a.x, a.y, b.x, b.y} {
  //
}

template <typename T>
constexpr Mat2<T>::Mat2(T m00, T m01, T m10, T m11) :
  elements{m00, m10, m01, m11} {
  //
}

template <typename T>
const T& Mat2<T>::operator()(int r, int c) const {
  assert(r >= 0 && r < 2 && c >= 0 && c < 2);
  return elements[c * 2 + r];
}

template <typename T>
T& Mat2<T>::operator()(int r, int c) {
  assert(r >= 0 && r < 2 && c >= 0 && c < 2);
  return elements[c * 2 + r];
}

template <typename T>
Mat2<T> operator*(const Mat2<T>& a, const Mat2<T>& b) {
  Mat2<T> result;

  result.elements[0] = a.elements[0] * b.elements[0] + a.elements[2] * b.elements[1];
  result.elements[1] = a.elements[1] * b.elements[0] + a.elements[3] * b.elements[1];

  result.elements[2] = a.elements[0] * b.elements[2] + a.elements[2] * b.elements[3];
  result.elements[3] = a.elements[1] * b.elements[2] + a.elements[3] * b.elements[3];

  return result;
}

template <typename T>
Vec2<T> operator*(const Mat2<T>& a, const Vec2<T>& b) {
  Vec2<T> result;
  result.x = a.elements[0] * b.x + a.elements[2] * b.y;
  result.y = a.elements[1] * b.x + a.elements[3] * b.y;
  return result;
}

template <typename T>
inline Mat2<T> inverse(const Mat2<T>& a) {
  Mat2<T> result;

  auto det = a.elements[0] * a.elements[3] - a.elements[1] * a.elements[2];
  auto inv_det = T(1) / det;

  result.elements[0] = inv_det * a.elements[3];
  result.elements[1] = -inv_det * a.elements[1];
  result.elements[2] = -inv_det * a.elements[2];
  result.elements[3] = inv_det * a.elements[0];

  return result;
}

template <typename T>
Mat2<T> transpose(const Mat2<T>& a) {
  //  @NOTE: Elements swap in ctor.
  return Mat2<T>{a.elements[0], a.elements[1], a.elements[2], a.elements[3]};
}

using Mat2f = Mat2<float>;

}