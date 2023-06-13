#include "SeasonComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

struct SeasonComponent {
  season::Status status{};
  Stopwatch state_timer;
  SeasonComponentParams params{};
  bool initialized{};
};

namespace {

struct {
  SeasonComponent component;
} globals;

} //  anon

SeasonComponent* get_global_season_component() {
  return &globals.component;
}

season::Status get_current_season_status(const SeasonComponent* comp) {
  return comp->status;
}

SeasonComponentParams* get_season_component_params(SeasonComponent* component) {
  return &component->params;
}

SeasonComponentUpdateResult update_season_component(SeasonComponent* component) {
  SeasonComponentUpdateResult result{};

  season::Events events{};

  if (!component->initialized) {
    component->state_timer.reset();
    component->initialized = true;
  }

  auto& status = component->status;
  auto& params = component->params;

  if (params.immediate_set_next) {
    events.just_jumped_to_state = true;
    status.frac_next = 0.0f;
    status.transitioning = false;
    status.current = params.immediate_set_next.value();
    status.next = status.current == season::Season::Summer ?
      season::Season::Fall : season::Season::Summer;
    params.immediate_set_next = NullOpt{};
    component->state_timer.reset();
  }

  if (params.update_enabled) {
    double elapsed_time = component->state_timer.delta().count();
    if (status.transitioning) {
      const double trans_time = 10.0;
      status.frac_next = float(clamp01(elapsed_time / trans_time));
      if (status.frac_next == 1.0) {
        status.frac_next = 0.0f;
        status.transitioning = false;
        events.just_finished_transition = true;
        std::swap(status.current, status.next);
        component->state_timer.reset();
      }
    } else {
      const double state_time = 10.0;
      if (elapsed_time >= state_time) {
        status.transitioning = true;
        events.just_began_transition = true;
        component->state_timer.reset();
      }
    }
  }

  result.status_and_events.status = status;
  result.status_and_events.events = events;
  return result;
}

GROVE_NAMESPACE_END
