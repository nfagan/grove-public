#pragma once

#include "grove/math/vector.hpp"
#include <memory>

namespace grove {

struct SkyGradient {
  using Data = std::unique_ptr<float[]>;

  struct Params {
    float y0{0.0f};
    float y1{0.25f};
    float y2{0.75f};
    float y3{1.0f};

    Vec3f y0_color{1.0f};
    Vec3f y1_color{1.0f};
    Vec3f y2_color{0.0f};
    Vec3f y3_color{0.0f};

    int texture_size{128};

    friend bool operator==(const Params& a, const Params& b);
    friend bool operator!=(const Params& a, const Params& b);
  };

public:
  void evaluate(const Params& params);

public:
  Data data;
  int size{};
};

inline bool operator==(const SkyGradient::Params& a, const SkyGradient::Params& b) {
  return a.y0 == b.y0 &&
         a.y1 == b.y1 &&
         a.y2 == b.y2 &&
         a.y3 == b.y3 &&
         a.y0_color == b.y0_color &&
         a.y1_color == b.y1_color &&
         a.y2_color == b.y2_color &&
         a.y3_color == b.y3_color &&
         a.texture_size == b.texture_size;
}

inline bool operator!=(const SkyGradient::Params& a, const SkyGradient::Params& b) {
  return !(a == b);
}

}