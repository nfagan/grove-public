#pragma once

#include "grove/common/Optional.hpp"
#include "grove/common/History.hpp"
#include <string>

namespace grove {

class ProfileComponent;

namespace vk {

class Profiler;

} //  vk

class ProfileComponentGUI {
public:
  struct UpdateResult {
    Optional<std::string> add_profile;
    Optional<std::string> remove_profile;
    Optional<std::string> add_gfx_profile;
    Optional<std::string> remove_gfx_profile;
    Optional<bool> enable_gpu_profiler;
    bool close_window{};
  };

  UpdateResult render(const ProfileComponent& component,
                      const vk::Profiler& gfx_profiler,
                      double audio_cpu_usage);

private:
  History<double, 32> audio_cpu_history;
};

}