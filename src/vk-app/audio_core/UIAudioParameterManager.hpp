#pragma once

#include "grove/audio/audio_parameters.hpp"
#include "grove/audio/audio_events.hpp"
#include "grove/common/Optional.hpp"
#include <unordered_map>
#include <vector>

namespace grove {

struct UIParameterChangeList {
  void clear() {
    parameter_change_events.clear();
  }
  size_t size() const {
    return parameter_change_events.size();
  }

  std::vector<AudioEvent> parameter_change_events;
};

union UIAudioParameterData {
  float f;
  int i;
};

struct UIAudioParameter {
public:
  float float_span() const;
  float fractional_value() const;
  bool is_float() const {
    return type == AudioParameterType::Float;
  }
  bool is_int() const {
    return type == AudioParameterType::Int;
  }
  bool is_continuous() const {
    return is_float();
  }
  bool updated_this_frame() const {
    return num_updates_this_frame > 0;
  }
  void set_target(const AudioParameterValue& value);
  void set_value(const AudioParameterValue& value);
  AudioParameterValue as_audio_parameter_value() const;

  static UIAudioParameter from_descriptor(const AudioParameterDescriptor& descriptor);

public:
  AudioParameterType type{};
  UIAudioParameterData value{};
  UIAudioParameterData target{};
  UIAudioParameterData min{};
  UIAudioParameterData max{};
  float time_to_change{};
  int num_updates_this_frame{};
};

/*
 * UIAudioParameterManager
 */

class UIAudioParameterManager {
public:
  //  At beginning of frame.
  void update(const UIParameterChangeList& change_list, double current_stream_time, double sample_rate);

  Optional<UIAudioParameter> read_value(AudioParameterIDs id) const;
  Optional<UIAudioParameter> require_and_read_value(const AudioParameterDescriptor& descriptor);

  void add_active_ui_parameter(AudioParameterIDs id, UIAudioParameter value);
  void remove_active_ui_parameter(AudioParameterIDs id);

  int num_active_ui_parameters() const {
    return int(active_ui_parameters.size());
  }
  int num_pending_events() const {
    return int(pending_events.size());
  }

private:
  double tick(double current_stream_time);
  void process_events(const UIParameterChangeList& change_list,
                      double current_stream_time,
                      double sample_rate);
  void update_parameter_values(double dt);
  void reset_parameter_update_counts();

private:
  std::unordered_map<AudioParameterIDs, UIAudioParameter, AudioParameterIDs::Hash> active_ui_parameters;
  std::vector<AudioEvent> pending_events;

  bool first_update = true;
  double last_update_stream_time = 0.0;
};

}