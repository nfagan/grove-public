#pragma once

#include "grove/math/random.hpp"
#include <cassert>

namespace grove::points {

namespace detail {

template <typename Float>
struct Random {};

template <>
struct Random<float> {
  static float rand() {
    return urandf();
  }
};

template <>
struct Random<double> {
  static double rand() {
    return urand();
  }
};

template <typename Vector, typename Float, int VecSize>
void randn(int n, Vector* dst) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < VecSize; j++) {
      dst[i][j] = points::detail::Random<Float>::rand();
    }
  }
}

template <typename Vec>
struct DefaultVectorTraits {
  static auto distance(const Vec& a, const Vec& b) {
    return (a - b).length();
  }
};

} //  points::detail

template <typename Float>
Float place_outside_radius_default_radius(int n, Float scale) {
  return scale * (Float(1) / std::sqrt(Float(2))) * std::sqrt(Float(1) / Float(n));
}

//  Vec* dst:     size = n
//  bool* accept: size = n
//  Float r:      radius in range [0, 1)
template <typename Vec, typename Float, int VecSize,
          typename VecTraits = points::detail::DefaultVectorTraits<Vec>>
void place_outside_radius(Vec* dst, bool* accept, int n, Float r, int iteration_limit = -1) {
  int num_left = n;
  int num_kept{};
  int iter{};
  while (num_left > 0 && (iteration_limit < 0 || iter < iteration_limit)) {
    assert(num_left + num_kept == n);

    points::detail::randn<Vec, Float, VecSize>(num_left, dst + num_kept);
    std::fill(accept, accept + n, true);

    for (int i = 0; i < n; i++) {
      if (!accept[i]) {
        continue;
      }

      bool any_gt{};
      for (int j = 0; j < n; j++) {
        if (i != j) {
          const Float len = VecTraits::distance(dst[i], dst[j]);
          if (len <= r) {
            accept[j] = false;
          } else {
            any_gt = true;
          }
        }
      }
      accept[i] = any_gt;
    }

    int num_accepted{};
    for (int i = 0; i < n; i++) {
      if (accept[i]) {
        dst[num_accepted++] = dst[i];
      }
    }

    num_kept = num_accepted;
    num_left = n - num_kept;
    iter++;
  }
#ifdef GROVE_DEBUG
  //  validate
  if (iteration_limit < 0) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        if (i != j) {
          const Float len = VecTraits::distance(dst[i], dst[j]);
          assert(len > r);
        }
      }
    }
  }
#endif
}

}