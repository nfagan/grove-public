#pragma once

#include "grove/math/Vec4.hpp"

namespace grove::cull {

struct FrustumCullInstance {
  Vec4f aabb_p0;
  Vec4f aabb_p1;
};

struct FrustumCullResult {
  uint32_t result;
};

struct FrustumCullGroupOffset {
  uint32_t offset;
};

static_assert(sizeof(FrustumCullResult) == 4);
static_assert(sizeof(FrustumCullInstance) == 32);

}