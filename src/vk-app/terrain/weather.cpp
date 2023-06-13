#include "weather.hpp"
#include "../weather/common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

terrain::GlobalRenderParams weather::terrain_render_params_from_status(const Status& status) {
  Optional<float> weather_status_t;
  if (status.current == weather::State::Sunny &&
      status.next == weather::State::Overcast) {
    weather_status_t = status.frac_next;
    //
  } else if (status.current == weather::State::Overcast &&
             status.next == weather::State::Sunny) {
    weather_status_t = 1.0f - status.frac_next;
  } else {
    //  @TODO
    assert(false);
  }

  terrain::GlobalRenderParams result{};
  if (weather_status_t) {
    auto t = weather_status_t.value();
    result.min_shadow = lerp(t, 0.5f, 0.95f);
    result.global_color_scale = lerp(t, 1.0f, 0.75f);
    result.frac_global_color_scale = 1.0f - t;
  }

  return result;
}

GROVE_NAMESPACE_END

