#include "gauss_distributed_pitches_impl.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace gdp;

inline double gauss_pdf(double t, double sigma) {
  return std::exp(-0.5 * std::pow(t / sigma, 2.0)) / (std::sqrt(2.0 * grove::pi()) * sigma);
}

void assign_probabilities(
  Distribution* dist, const float* mus, const float* sigmas, const float* scales, int num_lobes) {
  //
  std::fill(dist->p, dist->p + Config::st_buffer_size, 0.0);

  const int n_half_width = Config::st_gauss_half_width * Config::st_div;
  const auto half_width_st = double(Config::st_gauss_half_width);

  for (int i = 0; i < num_lobes; i++) {
    for (int h = 0; h < 2; h++) {
      //  left and right halves
      const double h_t_off = h == 0 ? -3.96 : 0.0;
      const double h_st_off = h == 0 ? -half_width_st : 0.0;

      for (int j = 0; j < n_half_width; j++) {
        double f = double(j) / double(n_half_width);
        double t = f * 3.96 + h_t_off;
        double st_off = f * half_width_st + mus[i] + h_st_off;
        const double raw_ind = std::round(
          (st_off - double(Config::root_st)) * double(Config::st_div)) + 1.0;
        const int ind = int(clamp(raw_ind, 0.0, double(Config::st_buffer_size - 1)));
        dist->p[ind] += double(scales[i]) * gauss_pdf(t, sigmas[i]);
      }
    }
  }
}

void prepare_for_sampling(Distribution* dist) {
  std::iota(dist->tmp_i, dist->tmp_i + Config::st_buffer_size, 0);
  std::sort(dist->tmp_i, dist->tmp_i + Config::st_buffer_size, [dist](int a, int b) {
    return dist->p[a] < dist->p[b];
  });

  for (int i = 0; i < Config::st_buffer_size; i++) {
    const int ind = dist->tmp_i[i];
    dist->sorted_p[i] = dist->p[ind];
    dist->sorted_z[i] = dist->z[ind];
  }

  for (int i = 1; i < Config::st_buffer_size; i++) {
    dist->sorted_p[i] = dist->sorted_p[i - 1] + dist->sorted_p[i];
  }

  double mx = dist->sorted_p[Config::st_buffer_size - 1];
  assert(mx >= 0.0);
  if (mx == 0.0) {
    mx = 1.0;
  }

  for (auto& p : dist->sorted_p) {
    p /= mx;
  }
}

} //  anon

void gdp::initialize(Distribution* dist) {
  int ind{};
  for (int i = 0; i < Config::oct_span; i++) {
    for (int j = 0; j < 12; j++) {
      for (int k = 0; k < Config::st_div; k++) {
        const int st = i * 12 * Config::st_div + j * Config::st_div + k;
        dist->z[ind++] = float(st) / float(Config::st_div) + float(Config::root_st);
      }
    }
  }

  assert(ind == Config::st_buffer_size);
  std::copy(dist->z, dist->z + Config::st_buffer_size, dist->sorted_z);
  std::fill(dist->p, dist->p + Config::st_buffer_size, 0.0);
  std::fill(dist->sorted_p, dist->sorted_p + Config::st_buffer_size, 0.0);
}

void gdp::update(
  Distribution* dist, const float* mus, const float* sigmas, const float* scales, int num_lobes) {
  //
  assign_probabilities(dist, mus, sigmas, scales, num_lobes);
  prepare_for_sampling(dist);
}

GROVE_NAMESPACE_END
