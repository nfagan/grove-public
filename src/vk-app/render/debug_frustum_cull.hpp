#pragma once

#include "grove/math/Vec3.hpp"

namespace grove::cull {

struct FrustumCullData;

namespace debug {

void draw_frustum_cull_data(const FrustumCullData* sys, const Vec3f& color);

}

}