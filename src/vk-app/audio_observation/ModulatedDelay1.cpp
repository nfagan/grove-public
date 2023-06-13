#include "ModulatedDelay1.hpp"
#include "../audio_core/UIAudioParameterManager.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

using namespace observe;

AudioParameterMonitor::MonitorableNode
ModulatedDelay1::make_node(OnNewParameterValue on_lfo_change) {
  auto wrap_change = [change = std::move(on_lfo_change)](const AudioParameterDescriptor&,
                                                         const UIAudioParameter& param) {
    if (change) {
      change(param.fractional_value());
    }
  };

  auto cb_method = AudioParameterMonitor::CallbackMethod::OnChange;
  auto param = AudioParameterMonitor::make_pending_monitorable_parameter(
    "lfo_representation", std::move(wrap_change), cb_method);

  AudioParameterMonitor::MonitorableNode result;
  result.params.push_back(std::move(param));
  return result;
}

GROVE_NAMESPACE_END