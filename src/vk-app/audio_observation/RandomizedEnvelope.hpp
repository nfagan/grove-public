#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class RandomizedEnvelope {
  using ValueCB = AudioParameterMonitor::ValueCallback;
public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB&& env_rep_cb);
};

}