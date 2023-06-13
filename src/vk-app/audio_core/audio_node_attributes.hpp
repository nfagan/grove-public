#pragma once

#include "AudioNodeStorage.hpp"
#include "grove/math/vector.hpp"

namespace grove {

class Camera;

Vec3f color_for_data_type(AudioNodeStorage::DataType type);

constexpr Vec3f color_for_isolating_ports() {
  return Vec3f{4.0f/255.0f, 154.0f/255.0f, 207.0f/255.0f};
}

void debug_draw_port(const AudioNodeStorage::PortInfo& port_info,
                     const Camera& camera,
                     const Vec3f& position,
                     const Vec3f& scale,
                     const Vec3f& input_scale,
                     bool selected);

}