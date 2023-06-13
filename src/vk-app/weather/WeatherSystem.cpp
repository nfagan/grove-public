#include "WeatherSystem.hpp"
#include "common.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "WeatherSystem";
}

} //  anon

struct WeatherSystemImpl {
public:
  enum class State {
    Idle,
    Transitioning,
    Stationary
  };

public:
  WeatherSystemImpl() {
    status.current = weather::State::Sunny;
    status.next = weather::State::Overcast;
    status.changed = true;
  }

public:
  weather::Status status{};
  Stopwatch stopwatch;
  State state{State::Stationary};
  bool first_update{true};
  bool update_enabled{true};
  Optional<weather::State> immediate_next_state;
  Optional<float> manually_set_frac_next;
  bool immediate_transition{};
  double stationary_t{180.0};
};

weather::Status WeatherSystem::update() {
  auto& status = impl->status;
#if 1
  if (impl->first_update) {
    impl->stopwatch.reset();
    impl->first_update = false;
    return status;
  }

  if (impl->immediate_next_state) {
    auto immediate_next = impl->immediate_next_state.value();
    auto new_next = immediate_next == status.current ? status.next : status.current;
    status.current = immediate_next;
    status.next = new_next;
    status.frac_next = 0.0f;
    impl->state = WeatherSystemImpl::State::Stationary;
    impl->stopwatch.reset();
    impl->immediate_next_state = NullOpt{};
    return status;
  }

  if (impl->manually_set_frac_next) {
    impl->state = WeatherSystemImpl::State::Transitioning;
    impl->stopwatch.reset();
    status.frac_next = impl->manually_set_frac_next.value();
    impl->manually_set_frac_next = NullOpt{};
    return status;
  }

  if (!impl->update_enabled) {
    return status;
  }

  double elapsed_t = impl->stopwatch.delta().count();
  const double transition_t = 20.0;
  const double stationary_t = impl->stationary_t;

  if (impl->immediate_transition) {
    impl->immediate_transition = false;
    if (impl->state == WeatherSystemImpl::State::Stationary) {
      elapsed_t = stationary_t;
    }
  }

  if (impl->state == WeatherSystemImpl::State::Stationary) {
    if (elapsed_t >= stationary_t) {
      GROVE_LOG_INFO_CAPTURE_META("Beginning transition", logging_id());
      impl->stopwatch.reset();
      impl->state = WeatherSystemImpl::State::Transitioning;
    }
  } else if (impl->state == WeatherSystemImpl::State::Transitioning) {
    if (elapsed_t >= transition_t) {
      GROVE_LOG_INFO_CAPTURE_META("Beginning stationary", logging_id());
      impl->stopwatch.reset();
      auto curr = status.current;
      status.current = status.next;
      status.next = curr;
      status.frac_next = 0.0f;
      impl->state = WeatherSystemImpl::State::Stationary;
    } else {
      status.frac_next = float(elapsed_t / transition_t);
    }
  }
#else
  status.frac_next = 0.0f;
  status.changed = impl->first_update;
  impl->first_update = false;
#endif

  return status;
}

WeatherSystem::WeatherSystem() : impl{new WeatherSystemImpl()} {
  //
}

WeatherSystem::~WeatherSystem() {
  delete impl;
}

void WeatherSystem::set_update_enabled(bool v) {
  impl->update_enabled = v;
}

void WeatherSystem::set_stationary_time(double t) {
  impl->stationary_t = t;
}

double WeatherSystem::get_stationary_time() const {
  return impl->stationary_t;
}

void WeatherSystem::set_immediate_state(weather::State state) {
  impl->immediate_next_state = state;
}

void WeatherSystem::begin_transition() {
  impl->immediate_transition = true;
}

const weather::Status& WeatherSystem::get_status() const {
  return impl->status;
}

void WeatherSystem::set_frac_next_state(float v) {
  impl->manually_set_frac_next = clamp01_open(v);
}

bool WeatherSystem::get_update_enabled() const {
  return impl->update_enabled;
}

GROVE_NAMESPACE_END
