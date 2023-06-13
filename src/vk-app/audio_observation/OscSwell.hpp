#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class OscSwell {
  using ValueCB = AudioParameterMonitor::ValueCallback;
public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB&& signal_rep_cb);
};

}