#pragma once

#include "AudioParameterMonitor.hpp"

namespace grove::observe {

class RandomizedSynths {
public:
  using ValueCB = AudioParameterMonitor::ValueCallback;

public:
  static AudioParameterMonitor::MonitorableNode make_node(ValueCB on_envelope_change,
                                                          ValueCB on_new_note_number);
};

}