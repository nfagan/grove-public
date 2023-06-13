#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class ModulatedDelay1 {
public:
  using OnNewParameterValue = std::function<void(float)>;

public:
  static AudioParameterMonitor::MonitorableNode make_node(OnNewParameterValue on_lfo_change);
};

}