#pragma once

#include "grove/common/profile.hpp"

namespace grove {

struct AppProfiling {
public:
  using ActiveSamples = std::unordered_map<std::string, profile::Samples>;
  using GraphicsActiveSamples = std::unordered_set<std::string>;

public:
  void add_active(const std::string& profile_id);
  void remove_active(const std::string& profile_id);

  void add_graphics_active(const std::string& profile_id);
  void remove_graphics_active(const std::string& profile_id);

  void update();
  const ActiveSamples& read_active_samples() const {
    return active_profile_samples;
  }

  const GraphicsActiveSamples& read_active_graphics_samples() const {
    return active_graphics_profile_samples;
  }

public:
  profile::Listener profiler_listener;
  ActiveSamples active_profile_samples;
  GraphicsActiveSamples active_graphics_profile_samples;
};

void initialize_common_profile_identifiers(AppProfiling& profiler);

}