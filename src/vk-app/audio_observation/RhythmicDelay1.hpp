#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class RhythmicDelay1 {
  using ValueCB = AudioParameterMonitor::ValueCallback;

public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB&& signal_rep_cb);
};

}