#include "profiling.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

void AppProfiling::add_active(const std::string& profile_id) {
  active_profile_samples[profile_id] = {};
}

void AppProfiling::remove_active(const std::string& profile_id) {
  active_profile_samples.erase(profile_id);
}

void AppProfiling::add_graphics_active(const std::string& profile_id) {
  active_graphics_profile_samples.insert(profile_id);
}

void AppProfiling::remove_graphics_active(const std::string& profile_id) {
  active_graphics_profile_samples.erase(profile_id);
}

void AppProfiling::update() {
  for (auto& active : active_profile_samples) {
    auto& identifier = active.first;
    GROVE_PROFILE_REQUEST(profiler_listener, identifier);
  }

  for (auto& active : active_profile_samples) {
    auto maybe_update_info =
      profiler_listener.find_first_query_match(active.first);

    if (maybe_update_info) {
      active.second = maybe_update_info->samples;
    }
  }

  profiler_listener.update();
}

void initialize_common_profile_identifiers(AppProfiling& profiler) {
  profiler.add_active("AudioRenderer/render");
  profiler.add_active("App/update");
  profiler.add_active("App/render");
  profiler.add_active("App/main_loop");

  profiler.add_graphics_active("GrassComponent/render");
  profiler.add_graphics_active("ProceduralTreeComponent/render");
  profiler.add_graphics_active("vsm/render");
}

GROVE_NAMESPACE_END
