#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class RandomizedInstrumentNodes {
public:
  using OnNewParameterValue = std::function<void(float)>;

public:
  static AudioParameterMonitor::MonitorableNode make_node(OnNewParameterValue on_signal_change,
                                                          OnNewParameterValue on_note_change);
};

}