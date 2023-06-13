#pragma once

#include "gauss_distributed_pitches.hpp"
#include "grove/math/constants.hpp"
#include <cmath>
#include <algorithm>
#include <cassert>

namespace grove::gdp {

void update(
  Distribution* dist, const float* mus, const float* sigmas, const float* scales, int num_lobes);

void initialize(Distribution* dist);

inline float sample(const Distribution* dist, double r) {
  assert(std::is_sorted(dist->sorted_p, dist->sorted_p + Config::st_buffer_size));
  auto it = std::upper_bound(dist->sorted_p, dist->sorted_p + Config::st_buffer_size, r);
  const int ind = std::min(Config::st_buffer_size - 1, int(it - dist->sorted_p));
  return dist->sorted_z[ind];
}

}