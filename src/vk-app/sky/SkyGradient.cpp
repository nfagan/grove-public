#include "SkyGradient.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Vec3<double> to_double(const Vec3f& v) {
  return Vec3<double>{v.x, v.y, v.z};
}

void apply(SkyGradient::Data& data, const SkyGradient::Params& params) {
  for (int i = 0; i < params.texture_size; i++) {
    auto y = 1.0 - double(i) / double(params.texture_size);
    double t;
    Vec3<double> a;
    Vec3<double> b;

    if (y < params.y1) {
      t = (y - params.y0) / (params.y1 - params.y0);
      a = to_double(params.y0_color);
      b = to_double(params.y1_color);

    } else if (y < params.y2) {
      t = (y - params.y1) / (params.y2 - params.y1);
      a = to_double(params.y1_color);
      b = to_double(params.y2_color);

    } else {
      t = (y - params.y2) / (params.y3 - params.y2);
      a = to_double(params.y2_color);
      b = to_double(params.y3_color);
    }

    auto v = clamp_each(lerp(t, a, b), Vec3<double>{0.0}, Vec3<double>{1.0});

    for (int j = 0; j < params.texture_size; j++) {
      int ind = (i * params.texture_size + j) * 4;

      for (int k = 0; k < 3; k++) {
        data[ind + k] = float(v[k]);
      }

      data[ind + 3] = 1.0f;  //  alpha
    }
  }
}

} //  anon

void SkyGradient::evaluate(const Params& params) {
  const auto new_size = params.texture_size * params.texture_size * 4;
  if (size != new_size) {
    data = std::make_unique<float[]>(new_size);
    size = new_size;
  }
  apply(data, params);
}

GROVE_NAMESPACE_END
