#pragma once

#include <cassert>
#include <cmath>

namespace grove {

struct MoogLPFilterState {
public:
  void update(double sample_rate, float cut, float res);
  float tick(float curr);

public:
  float y1{};
  float y2{};
  float y3{};
  float y4{};
  float last_x{};
  float last_y1{};
  float last_y2{};
  float last_y3{};
  float x{};
  float r{};
  float p{};
  float k{};
};

/*
 * impl
 */

inline void MoogLPFilterState::update(double sample_rate, float cut, float res) {
  auto f = float((cut + cut) / sample_rate);
  p = f * (1.8f - 0.8f * f);
  k = p + p - 1.0f;
//  k = 2.0 * std::sin(f * pi() * 0.5) - 1.0;

  auto t = (1.0f - p) * 1.386249f;
  auto t2 = 12.0f + t * t;
  r = res * (t2 + 6.0f * t) / (t2 - 6.0f * t);
}

inline float MoogLPFilterState::tick(float curr) {
  x = curr - r * y4;

  y1 = x * p + last_x * p - k * y1;
  y2 = y1 * p + last_y1 * p - k * y2;
  y3 = y2 * p + last_y2 * p - k * y3;
  y4 = y3 * p + last_y3 * p - k * y4;

  y4 -= (y4 * y4 * y4) / 6.0f;
  assert(std::isfinite(y4));

  last_x = x;
  last_y1 = y1;
  last_y2 = y2;
  last_y3 = y3;

  return y4;
}

}