#pragma once

#include "SampledImageManager.hpp"

namespace grove::vk {

class NoiseImages {
public:
  struct InitInfo {
    SampledImageManager& image_manager;
  };
public:
  void initialize(const InitInfo& init_info);

public:
  Optional<SampledImageManager::Handle> bayer8;
};

}