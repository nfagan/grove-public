#include "fdft.hpp"
#include <cmath>
#include <cassert>
#include <cstring>

namespace grove {

namespace {

constexpr double two_pi = 6.28318530717958647692528676655900576;
constexpr int max_num_levels = 10;
constexpr int max_size = 1 << (max_num_levels + 1);
double twiddles[max_num_levels][max_size];

template <typename T>
void cmul(T a, T ci, T b, T dj, T* oa, T* oi) {
  //  (a + ci)(b + dj)
  //  ab + adj + cbi + cdji
  //  (ab - cd) + (adj + cbi)
  *oa = a * b - ci * dj;
  *oi = a * dj + ci * b;
}

template <typename T>
void fdft(T* out, const T* in, int n, int s, int level) {
  if (n == 1) {
    out[0] = in[0];
  } else {
    assert(level > 0);

    const int n2 = n >> 1;
    const int s2 = s << 1;

    fdft(out, in, n2, s2, level - 1);
    fdft(out + n, in + s, n2, s2, level - 1);  //  would be out + n/2, but * 2

    for (int k = 0; k < n2; k++) {
      T* p = out + k * 2;
      T* q = out + (k * 2 + n);

      const T p0 = p[0];
      const T p1 = p[1];
#if 1
      const T re = T(twiddles[level - 1][k * 2 + 0]);
      const T im = T(twiddles[level - 1][k * 2 + 1]);
#else
      (void) level;
      const T w = T(-grove::two_pi()) / T(n) * T(k);
      const T re = std::cos(w);
      const T im = std::sin(w);
#endif
      T q0;
      T q1;
      cmul(re, im, q[0], q[1], &q0, &q1);

      p[0] = p0 + q0;
      p[1] = p1 + q1;
      q[0] = p0 - q0;
      q[1] = p1 - q1;
    }
  }
}

} //  anon

void init_fdft() {
  for (int i = 0; i < max_num_levels; i++) {
    const int n = 1 << (i + 1);
    for (int j = 0; j < n / 2; j++) {
      const double w = -two_pi / double(n) * double(j);
      twiddles[i][j * 2 + 0] = std::cos(w);
      twiddles[i][j * 2 + 1] = std::sin(w);
    }
  }
}

void fdft(float* out, const float* in, int n) {
  if (n > 0) {
    assert((n & (n - 1)) == 0 && n <= max_size);
    const int level = int(std::log2(double(n)));
    memset(out, 0, n * 2 * sizeof(float));
    fdft(out, in, n, 1, level);
  }
}

void fdft(double* out, const double* in, int n) {
  if (n > 0) {
    assert((n & (n - 1)) == 0 && n <= max_size);
    const int level = int(std::log2(double(n)));
    memset(out, 0, n * 2 * sizeof(double));
    fdft(out, in, n, 1, level);
  }
}

}