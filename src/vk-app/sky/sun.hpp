#pragma once

#include "grove/math/vector.hpp"

namespace grove {

struct Sun {
  Vec3f position{10.0f, 50.0f, 100.0f};
  Vec3f color{1.0f};
};

namespace sun {

Vec3<double> compute_position(double theta_01, double phi_radians, double distance);

}

}