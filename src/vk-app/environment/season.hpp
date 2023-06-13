#pragma once

namespace grove::season {

enum class Season {
  Summer = 0,
  Fall
};

struct Events {
  bool just_began_transition{};
  bool just_finished_transition{};
  bool just_jumped_to_state{};
};

struct Status {
  Season current{Season::Summer};
  Season next{Season::Fall};
  float frac_next{};
  bool transitioning{};
};

struct StatusAndEvents {
  Status status;
  Events events;
};

inline const char* to_string(Season season) {
  switch (season) {
    case Season::Fall:
      return "Fall";
    case Season::Summer:
      return "Summer";
    default:
      return "Unknown";
  }
}

}