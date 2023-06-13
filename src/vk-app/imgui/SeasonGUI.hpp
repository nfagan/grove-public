#pragma once

#include "grove/common/Optional.hpp"

namespace grove {

struct SeasonComponent;

struct SeasonGUIUpdateResult {
  bool close{};
};

class SeasonGUI {
public:
  SeasonGUIUpdateResult render(SeasonComponent& component);
};

}