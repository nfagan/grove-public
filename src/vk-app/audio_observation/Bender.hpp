#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class Bender {
  using ValueCB = AudioParameterMonitor::ValueCallback;
public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB&& quant_rep_cb,
                                                          ValueCB&& signal_rep_cb);
};

}