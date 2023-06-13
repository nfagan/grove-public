#pragma once

#include "grove/math/vector.hpp"
#include <vector>

namespace grove {

struct CablePathObstacle {
  Vec2f position;
  float radius;
};

using CablePathObstacles = std::vector<CablePathObstacle>;

struct CablePathResult {
  bool success{};
  std::vector<Vec2f> path_positions;
  int64_t computed_in_num_iters{};
};

struct CablePath {
  uint32_t id{};
  std::vector<Vec2f> positions;
};

struct CablePathInstanceData {
public:
  explicit CablePathInstanceData(const CablePathObstacles* obstacles);

public:
  Vec2f source;
  Vec2f target;

  const CablePathObstacles* obstacles;
};

class CablePathFind {
public:
  static constexpr int64_t fail_if_open_set_reaches_size = int64_t(1e5);
  static constexpr int64_t fail_if_reaches_num_iterations = int64_t(1e5);
  static constexpr float large_grid_size = 2.0f;
  static constexpr float end_point_grid_size = 0.5f;

public:
  struct Parameters {
    float grid_cell_size = large_grid_size;
  };

public:
  static CablePathResult compute_path(CablePathInstanceData& instance,
                                      const Parameters& params);
};

}