#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class TriggeredOsc {
  using ValueCB = AudioParameterMonitor::ValueCallback;
public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB&& signal_rep, ValueCB&& note_num);
};

}