#include "ProfileComponentGUI.hpp"
#include "../util/ProfileComponent.hpp"
#include "../vk/profiler.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>
#include <vector>
#include <string>

GROVE_NAMESPACE_BEGIN

namespace {

inline std::string imgui_tagged_id(std::string base_label, const std::string& id) {
  base_label += "##";
  base_label += id;
  return base_label;
}

void render_audio_cpu_usage(History<double, 32>& history, double audio_cpu_usage_estimate) {
  history.push(audio_cpu_usage_estimate * 1e2);

  const auto cpu_usage_color = [audio_cpu_usage_estimate]() {
    if (audio_cpu_usage_estimate >= 1.0) {
      return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    } else if (audio_cpu_usage_estimate >= 0.5) {
      return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    } else if (audio_cpu_usage_estimate >= 0.25) {
      return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    } else {
      return ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
    }
  };

  ImGui::TextColored(cpu_usage_color(), "%% Audio CPU: %0.2f mean, %0.2f min, %0.2f max",
                     history.mean_or_default(0.0),
                     history.min_or_default(0.0),
                     history.max_or_default(0.0));
}

} //  anon

using UpdateResult = ProfileComponentGUI::UpdateResult;

UpdateResult ProfileComponentGUI::render(const ProfileComponent& component,
                                         const vk::Profiler& gfx_profiler,
                                         double audio_cpu_usage) {
  UpdateResult result{};

  ImGui::Begin("ProfileGUI");

  ImGui::Text("CPU");
  render_audio_cpu_usage(audio_cpu_history, audio_cpu_usage);

  const auto& profile_samples = component.profiler.read_active_samples();
  for (auto& [id, samples] : profile_samples) {
    auto remove_button_text = imgui_tagged_id("x", id);
    if (ImGui::SmallButton(remove_button_text.c_str())) {
      result.remove_profile = id;
    }
    ImGui::SameLine();
    ImGui::Text("%s (%d):\n%s", id.c_str(), samples.num_samples(), samples.stat_str().c_str());
  }

  constexpr int text_buff_size = 1024;
  char text_buffer[text_buff_size];
  memset(text_buffer, 0, text_buff_size);
  auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  if (ImGui::InputText("AddProfile", text_buffer, text_buff_size, enter_flag)) {
    std::string copy_buffer(text_buffer);
    result.add_profile = std::move(copy_buffer);
  }

  bool gpu_profile_enabled = gfx_profiler.is_enabled();
  if (ImGui::Checkbox("EnableGPUProfiler", &gpu_profile_enabled)) {
    result.enable_gpu_profiler = gpu_profile_enabled;
  }

  if (gpu_profile_enabled) {
    ImGui::Text("GPU");
    const auto& gfx_profile_samples = component.profiler.read_active_graphics_samples();
    for (auto& id : gfx_profile_samples) {
      auto remove_button_text = imgui_tagged_id("x", id);
      if (ImGui::SmallButton(remove_button_text.c_str())) {
        result.remove_gfx_profile = id;
      }

      ImGui::SameLine();
      if (auto entry = gfx_profiler.get(id)) {
        auto stat_str = entry->stat_str();
        ImGui::Text("%s (%d):\n%s", id.c_str(), entry->num_samples(), stat_str.c_str());
      } else {
        ImGui::Text("%s (%d):\n%s", id.c_str(), 0, "N/A");
      }
    }

    memset(text_buffer, 0, text_buff_size);
    if (ImGui::InputText("AddVKProfile", text_buffer, text_buff_size, enter_flag)) {
      std::string copy_buffer(text_buffer);
      result.add_gfx_profile = std::move(copy_buffer);
    }
  }

  if (ImGui::Button("Close")) {
    result.close_window = true;
  }

  ImGui::End();
  return result;
}

GROVE_NAMESPACE_END
