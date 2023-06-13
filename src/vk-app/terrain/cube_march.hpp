#pragma once

#include "grove/math/Vec3.hpp"
#include <cstdint>
#include <functional>
#include <vector>

#define GROVE_CUBE_MARCH_INCLUDE_NORMALS (1)

namespace grove::cm {

struct GenTrisParams {
  bool smooth;
};

struct GridInfo {
  Vec3f offset;
  Vec3f scale;
  Vec3f size;
};

using GenSurface = std::function<float(const Vec3f&)>;

void simple_grid_march_range(const GridInfo& grid, const GenSurface& gen_surface,
                             float thresh, const Vec3<int>& p0, const Vec3<int>& p1,
                             const GenTrisParams& params, std::vector<Vec3f>* ps,
                             std::vector<Vec3f>* ns);
Vec3f world_to_coord(const Vec3f& p, const GridInfo& grid);

}