#pragma once

#include "matrix.hpp"
#include "vector.hpp"
#include "constants.hpp"
#include <cmath>

namespace grove {

template <typename T>
Mat4<T> infinite_perspective_reverses_depth(T fovy, T s, T near) {
  Mat4<T> res(0);

  const auto g = T(1) / std::tan(fovy / T(2));
  const T e = Epsilon<T>::value;

  res(0, 0) = g / s;
  res(1, 1) = g;
  res(2, 2) = e;
  res(2, 3) = near * (T(1) - e);

  res(3, 2) = T(1);
  res(3, 3) = T(0);

  return res;
}

template <typename T>
Mat4<T> infinite_perspective(T fovy, T s, T near) {
  Mat4<T> res(0);

  const auto g = T(1) / std::tan(fovy / T(2));
  const T e = T(1) - Epsilon<T>::value;

  res(0, 0) = g / s;
  res(1, 1) = g;
  res(2, 2) = e;
  res(2, 3) = -near * e;

  res(3, 2) = T(1);
  res(3, 3) = 0;

  return res;
}

template <typename T>
Mat4<T> orthographic(T w, T h, T d) {
  Mat4<T> res(0);

  res(0, 0) = T(2) / w;
  res(1, 1) = T(2) / h;
  res(2, 2) = T(1) / d;
  res(3, 3) = T(1);

  return res;
}

template <typename T>
Mat4<T> look_at(const Vec3<T>& eye, const Vec3<T>& center, const Vec3<T>& world_up) {
  auto f = normalize(center - eye);
  auto r = normalize(cross(f, world_up));
  auto u = cross(r, f);

  return Mat4<T>(r.x, r.y, r.z, -dot(r, eye),
                 u.x, u.y, u.z, -dot(u, eye),
                 f.x, f.y, f.z, -dot(f, eye),
                 T(0), T(0), T(0), T(1));
}

template <typename T>
Mat4<T> make_translation(const Vec3<T>& pos) {
  Mat4<T> res(T(1));
  res(0, 3) = pos.x;
  res(1, 3) = pos.y;
  res(2, 3) = pos.z;
  return res;
}

template <typename T>
Mat4<T> make_scale(const Vec3<T>& scl) {
  Mat4<T> res(T(1));
  res(0, 0) = scl.x;
  res(1, 1) = scl.y;
  res(2, 2) = scl.z;
  return res;
}

template <typename T>
Mat4<T> make_translation_scale(const Vec3<T>& pos, const Vec3<T>& scl) {
  Mat4<T> res(T(1));

  res(0, 3) = pos.x;
  res(1, 3) = pos.y;
  res(2, 3) = pos.z;

  res(0, 0) = scl.x;
  res(1, 1) = scl.y;
  res(2, 2) = scl.z;

  return res;
}

template <typename T>
Mat3x4<T> make_translation_scale3x4(const Vec3<T>& pos, const Vec3<T>& scl) {
  Mat3x4<T> res(T(1));

  res(0, 3) = pos.x;
  res(1, 3) = pos.y;
  res(2, 3) = pos.z;

  res(0, 0) = scl.x;
  res(1, 1) = scl.y;
  res(2, 2) = scl.z;

  return res;
}

template <typename T>
Mat4<T> make_rotation(T angle, const Vec3<T>& a) {
  //  Due to Lengyel, E. Foundations of Game Engine Development Vol. 1, pp. 64-65
  const T c = std::cos(angle);
  const T s = std::sin(angle);
  const T d = T(1) - c;

  const T x = a.x * d;
  const T y = a.y * d;
  const T z = a.z * d;

  const T axay = x * a.y;
  const T axaz = x * a.z;
  const T ayaz = y * a.z;

  return Mat4<T>(c + x * a.x, axay - s * a.z, axaz * s + a.y, T(0),
                 axay + s * a.z, c + y * a.y, ayaz - s * a.x, T(0),
                 axaz - s * a.y, ayaz + s * a.x, c + z * a.z, T(0),
                 T(0), T(0), T(0), T(1));
}

template <typename T>
Mat4<T> make_x_rotation(T theta) {
  const T ct = std::cos(theta);
  const T st = std::sin(theta);

  Mat4<T> res(T(1));

  res(1, 1) = ct;
  res(2, 1) = st;
  res(1, 2) = -st;
  res(2, 2) = ct;

  return res;
}

template <typename T>
Mat4<T> make_y_rotation(T theta) {
  const T ct = std::cos(theta);
  const T st = std::sin(theta);

  Mat4<T> res(T(1));
  res(0, 0) = ct;
  res(0, 2) = st;

  res(2, 0) = -st;
  res(2, 2) = ct;

  return res;
}

template <typename T>
Mat4<T> make_z_rotation(T theta) {
  const T ct = std::cos(theta);
  const T st = std::sin(theta);

  Mat4<T> res(T(1));
  res(0, 0) = ct;
  res(1, 0) = st;

  res(0, 1) = -st;
  res(1, 1) = ct;

  return res;
}

template <typename T>
Mat2<T> make_rotation(T theta) {
  auto st = std::sin(theta);
  auto ct = std::cos(theta);
  return Mat2<T>(ct, -st, st, ct);
}

template <typename T>
Mat3<T> make_rotation3(T theta) {
  auto st = std::sin(theta);
  auto ct = std::cos(theta);
  return Mat3<T>(ct, -st, T(0),
                 st, ct, T(0),
                 T(0), T(0), T(1));
}

template <typename T>
Mat3<T> make_scale3(const Vec2<T>& s) {
  return Mat3<T>(s.x, T(0), T(0),
                 T(0), s.y, T(0),
                 T(0), T(0), T(1));
}

template <typename T>
Mat3<T> make_translation3(const Vec2<T>& t) {
  return Mat3<T>(T(1), T(0), t.x,
                 T(0), T(1), t.y,
                 T(0), T(0), T(1));
}

template <typename T>
Mat3<T> make_translation_scale3(const Vec2<T>& t, const Vec2<T>& s) {
  return Mat3<T>(s.x, T(0), t.x,
                 T(0), s.y, t.y,
                 T(0), T(0), T(1));
}

}