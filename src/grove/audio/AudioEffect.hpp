#pragma once

#include "types.hpp"
#include "audio_events.hpp"
#include "audio_parameters.hpp"
#include <vector>
#include <mutex>

namespace grove {

struct AudioParameterChangeView;

class AudioEffect {
public:
  virtual ~AudioEffect() = default;

  virtual void process(Sample* samples,
                       AudioEvents* events,
                       const AudioParameterChangeView& parameter_changes,
                       const AudioRenderInfo& info) = 0;

  virtual AudioParameterDescriptors parameter_descriptors() const;
  virtual AudioParameterID parameter_parent_id() const;

  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual bool is_enabled() const = 0;

  void toggle_enabled();
};

}