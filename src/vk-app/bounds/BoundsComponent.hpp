#pragma once

#include "bounds_system.hpp"

namespace grove {

struct SystemsGUIUpdateResult;

class BoundsComponent {
public:
  struct InitInfo {
    bounds::BoundsSystem* bounds_system;
  };

public:
  void initialize(const InitInfo& info);
  void on_gui_update(const SystemsGUIUpdateResult& gui_res);

public:
  bounds::AccelInstanceHandle default_accel{};
  bounds::CreateAccelInstanceParams create_accel_instance_params{};
};

}