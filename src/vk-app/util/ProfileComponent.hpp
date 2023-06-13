#pragma once

#include "profiling.hpp"
#include "../imgui/ProfileComponentGUI.hpp"

namespace grove {

class ProfileComponent {
  friend class ProfileComponentGUI;
public:
  void initialize();
  void update();
  void on_gui_update(const ProfileComponentGUI::UpdateResult& update_res);

private:
  AppProfiling profiler;
};

}