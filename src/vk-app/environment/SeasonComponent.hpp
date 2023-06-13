#pragma once

#include "season.hpp"
#include "grove/common/Optional.hpp"

namespace grove {

struct SeasonComponent;

struct SeasonComponentUpdateResult {
  season::StatusAndEvents status_and_events;
};

struct SeasonComponentParams {
  Optional<season::Season> immediate_set_next;
  bool update_enabled{};
};

SeasonComponent* get_global_season_component();
SeasonComponentUpdateResult update_season_component(SeasonComponent* component);
season::Status get_current_season_status(const SeasonComponent* component);
SeasonComponentParams* get_season_component_params(SeasonComponent* component);

}