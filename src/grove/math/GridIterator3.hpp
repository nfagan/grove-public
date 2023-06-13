#pragma once

#include "Vec3.hpp"

namespace grove {

template <typename T>
struct GridIterator3 {
  const Vec3<T>& operator*() const {
    return i;
  }

  GridIterator3<T>& operator++() {
    i.z++;
    if (i.z == end.z) {
      i.z = beg.z;
      i.y++;
    }
    if (i.y == end.y) {
      i.y = beg.y;
      i.x++;
    }
    return *this;
  }

  Vec3<T> i;
  Vec3<T> beg;
  Vec3<T> end;
};

template <typename T>
GridIterator3<T> begin_it(const Vec3<T>& beg, const Vec3<T>& end) {
  GridIterator3<T> res;
  res.i = beg;
  res.beg = beg;
  res.end = end;
  return res;
}

template <typename T>
bool is_valid(const GridIterator3<T>& it) {
  return it.i.x < it.end.x && it.i.y < it.end.y && it.i.z < it.end.z;
}

}