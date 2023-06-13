#include "ParticleGUI.hpp"
#include "../particle/PollenComponent.hpp"
#include "grove/common/common.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

bool ParticleGUI::render(const PollenComponent& comp) {
  bool close{};
  ImGui::Begin("ParticleGUI");

  ImGui::Text("NumPollenParticles: %d", int(comp.pollen_particles.num_particles()));

  if (ImGui::Button("Close")) {
    close = true;
  }

  ImGui::End();
  return close;
}

GROVE_NAMESPACE_END
