#include "weather.hpp"
#include "grove/common/common.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr Vec3f overcast_sun_color{0.1f};
  static constexpr Vec3f sunny_sun_color{1.0f};
};

SkyGradient::Params sunny_params() {
#if 0
  constexpr Vec3f blue{75.0f/255.0f, 143.0f/255.0f, 233.0f/255.0f};
  constexpr Vec3f white{249.0f/255.0f, 250.0f/255.0f, 241.0f/255.0f};

  SkyGradient::Params result{};
  result.y0_color = white;
  result.y1_color = white;
  result.y2_color = blue;
  result.y3_color = Vec3f{0.1f};
#else
  SkyGradient::Params result{};
  result.y0_color = Vec3f{1.0f};
  result.y1_color = Vec3f{1.0f};
  result.y2_color = Vec3f{0.05f, 0.545f, 233.0f/255.0f};
  result.y3_color = Vec3f{0.1f};
#endif

  result.y1 = 0.47f;
  result.y2 = 0.64f;

  return result;
}

SkyGradient::Params cloudy_params() {
  SkyGradient::Params result{};
  result.y0_color = Vec3f{0.5f};
  result.y1_color = Vec3f{0.9f, 0.9f, 0.85f};
  result.y2_color = Vec3f{0.7f};
  result.y3_color = Vec3f{0.1f};

  result.y1 = 0.47f;
  result.y2 = 0.64f;

  return result;
}

SkyGradient::Params lerp_params(float t,
                                const SkyGradient::Params& a,
                                const SkyGradient::Params& b) {
  SkyGradient::Params result = a;
  result.y0_color = lerp(t, a.y0_color, b.y0_color);
  result.y1_color = lerp(t, a.y1_color, b.y1_color);
  result.y2_color = lerp(t, a.y2_color, b.y2_color);
  result.y3_color = lerp(t, a.y3_color, b.y3_color);
  return result;
}

} //  anon

SkyGradient::Params weather::sunny_to_overcast_gradient_params(float frac_cloudy) {
  return lerp_params(frac_cloudy, sunny_params(), cloudy_params());
}

SkyGradient::Params weather::overcast_to_sunny_gradient_params(float frac_sunny) {
  return lerp_params(frac_sunny, cloudy_params(), sunny_params());
}

Vec3f weather::overcast_to_sunny_sun_color(float frac_sunny) {
  return lerp(frac_sunny, Config::overcast_sun_color, Config::sunny_sun_color);
}

Vec3f weather::sunny_to_overcast_sun_color(float frac_cloudy) {
  return lerp(frac_cloudy, Config::sunny_sun_color, Config::overcast_sun_color);
}

GROVE_NAMESPACE_END
