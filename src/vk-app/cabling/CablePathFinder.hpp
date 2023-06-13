#pragma once

#include "path_find.hpp"

namespace grove {

class CablePathFinder {
  using ObstacleID = uint64_t;

public:
  void add_obstacles(const Vec3f* positions,
                     int num_positions,
                     float radius,
                     const Vec2f& position_offset);

  ObstacleID add_obstacle(const Vec2f& position, float radius);
  void remove_obstacle(ObstacleID id);
  void modify_obstacle(ObstacleID by_id, const Vec2f& position, float radius);

public:
  CablePathResult compute_path(const Vec2f& source, const Vec2f& target) const;

private:
  std::vector<Vec2f> compute_path_end_point(const Vec2f& p0, const Vec2f& p1) const;
  ObstacleID add_obstacle(const CablePathObstacle& obstacle);

private:
  std::vector<CablePathObstacle> obstacles;
  std::vector<ObstacleID> obstacle_ids;
  uint64_t next_obstacle_id{1};
};

}