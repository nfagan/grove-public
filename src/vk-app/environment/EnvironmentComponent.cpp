#include "EnvironmentComponent.hpp"
#include "../weather/common.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

Tuning make_tuning(const weather::Status& weather_status, Tuning tuning) {
  float f = weather_status.frac_next;
  double f0 = 330.0;
  double f1 = 440.0f;
  if (weather_status.next == weather::State::Overcast) {
    std::swap(f0, f1);
  }
  tuning.reference_frequency = lerp(f * f * f * f, f0, f1);
  return tuning;
}

} //  anon

EnvironmentComponent::InitResult EnvironmentComponent::initialize() {
  InitResult result{};
  result.ambient_sound_init_res = ambient_sound.initialize();
  return result;
}

EnvironmentComponent::UpdateResult EnvironmentComponent::update(const UpdateInfo& update_info) {
  UpdateResult result{};

  auto& weather_status = update_info.weather_status;
  Optional<float> lerp_t;
  if (weather_status.current == weather::State::Sunny &&
      weather_status.next == weather::State::Overcast) {
    lerp_t = weather_status.frac_next;
  } else if (weather_status.current == weather::State::Overcast &&
             weather_status.next == weather::State::Sunny) {
    lerp_t = 1.0f - weather_status.frac_next;
  }

  if (lerp_t) {
    ambient_sound.set_rain_gain_frac(lerp_t.value());
  }

  if (weather_status.changed) {
    result.new_tuning = make_tuning(weather_status, update_info.current_tuning);
  }

  result.ambient_sound_update_res = ambient_sound.update({});
  return result;
}

GROVE_NAMESPACE_END
