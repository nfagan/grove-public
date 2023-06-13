#include "path_find.hpp"
#include "grove/common/common.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/math/random.hpp"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <queue>

GROVE_NAMESPACE_BEGIN

namespace {

using CellIndex = Vec2<int32_t>;
using CellKey = uint64_t;

struct Score {
  float f;
  float g;
};

template <typename T>
using ScoreMap = std::unordered_map<CellKey, T>;

inline CellIndex cell_index(const Vec2f& position, float grid_cell_size) {
  auto ind = grove::floor(position / grid_cell_size);
  return {int32_t(ind.x), int32_t(ind.y)};
}

inline uint64_t cell_index_key(const CellIndex& index) {
  uint64_t x{0};
  std::memcpy(&x, &index.x, sizeof(int32_t));

  uint64_t y{0};
  std::memcpy(&y, &index.y, sizeof(int32_t));

  y = y << 32;

  auto res = x;
  res |= y;

  return res;
}

inline CellIndex key_to_cell_index(uint64_t key) {
  uint64_t mask_x{~uint32_t(0)};

  uint64_t bits_x = key & mask_x;
  uint64_t bits_y = key & ~mask_x;

  bits_y = bits_y >> 32;

  CellIndex res;
  std::memcpy(&res.x, &bits_x, sizeof(int32_t));
  std::memcpy(&res.y, &bits_y, sizeof(int32_t));

  return res;
}

inline Vec2f cell_position(const CellIndex& index, float grid_cell_size) {
  Vec2f res{float(index.x), float(index.y)};
  return res * grid_cell_size;
}

inline bool point_obstacle_intersect(const CablePathObstacles& obstacles, const Vec2f& p) {
  for (const auto& obstacle : obstacles) {
    const auto& cp = obstacle.position;
    const auto& cr = obstacle.radius;

    if (point_circle_intersect(p, cp, cr)) {
      return true;
    }
  }
  return false;
}

inline bool ray_obstacle_intersect(const CablePathInstanceData& instance,
                                   const Vec2f& p0, const Vec2f& p1) {
  auto rd = p1 - p0;
  auto ro = p0;

  for (const auto& obstacle : *instance.obstacles) {
    float t0;
    float t1;

    if (ray_circle_intersect(ro, rd, obstacle.position, obstacle.radius, &t0, &t1) &&
        t0 >= 0 && t1 >= 0 && (t0 < 1 || t1 < 1)) {
      return true;
    }
  }

  return false;
}

inline float cost_function(const CablePathInstanceData& instance, const Vec2f& node_position) {
  const auto targ = instance.target;

  if (point_obstacle_intersect(*instance.obstacles, node_position)) {
    return grove::infinityf();
  } else {
    auto to_targ = node_position - targ;
    return to_targ.length();
  }
}

std::vector<Vec2f> reconstruct_path(const ScoreMap<CellKey>& came_from,
                                    CellIndex current,
                                    float grid_cell_size) {
  std::vector<Vec2f> result;

  while (true) {
    auto key = cell_index_key(current);

    if (came_from.count(key) == 0) {
      break;
    }

    auto from = key_to_cell_index(came_from.at(key));
    auto from_p = cell_position(from, grid_cell_size);
    result.insert(result.begin(), from_p);
    current = from;
  }

  return result;
}

} //  anon

struct ScoreComparator {
  ScoreComparator(const ScoreMap<Score>* scores) : scores(scores) {
    //
  }

  inline bool operator()(const CellIndex& a, const CellIndex& b) const noexcept {
    auto key_a = cell_index_key(a);
    auto key_b = cell_index_key(b);
    return scores->at(key_a).f > scores->at(key_b).f;
  }

  const ScoreMap<Score>* scores;
};

using OpenSet = std::priority_queue<CellIndex, std::vector<CellIndex>, ScoreComparator>;

/*
 * CablePathInstanceData
 */

CablePathInstanceData::CablePathInstanceData(const std::vector<CablePathObstacle>* obstacles) :
  obstacles(obstacles) {
  //
}

/*
 * compute_path
 */

CablePathResult CablePathFind::compute_path(CablePathInstanceData& instance,
                                            const Parameters& params) {
  CablePathResult result;
  result.success = false;
  result.computed_in_num_iters = -1;

  ScoreMap<CellKey> came_from;
  ScoreMap<Score> scores;
  std::unordered_set<CellKey> in_open_set;

  const auto cell_size = params.grid_cell_size;
  auto index_source = cell_index(instance.source, cell_size);
  auto key_source = cell_index_key(index_source);

  scores[key_source] = {
    cost_function(instance, cell_position(index_source, cell_size)),
    0
  };

  ScoreComparator score_comparator{&scores};
  OpenSet open_set{score_comparator};

  open_set.push(index_source);
  in_open_set.insert(key_source);

  int64_t num_iters = 0;

  while (!open_set.empty()) {
    num_iters++;

    auto current = open_set.top();
    open_set.pop();

    auto key_current = cell_index_key(current);
    auto g_current = scores.at(key_current).g;
    auto p_current = cell_position(current, cell_size);

    in_open_set.erase(key_current);
    assert(in_open_set.size() == open_set.size());

    if (in_open_set.size() >= CablePathFind::fail_if_open_set_reaches_size ||
        num_iters >= CablePathFind::fail_if_reaches_num_iterations) {
      return result;
    }

    bool success_stop_crit =
      std::abs(p_current.x - instance.target.x) <= cell_size &&
      std::abs(p_current.y - instance.target.y) <= cell_size;

    if (success_stop_crit) {
      result.success = true;
      result.computed_in_num_iters = num_iters;
      result.path_positions = reconstruct_path(came_from, current, cell_size);
      return result;
    }

    const int neighbor_offsets[3] = {-1, 0, 1};

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        auto off_x = neighbor_offsets[i];
        auto off_z = neighbor_offsets[j];

        if (off_x == 0 && off_z == 0) {
          continue;
        }

        CellIndex neighbor_offset{off_x, off_z};
        auto neighbor = current + neighbor_offset;
        auto p_neighbor = cell_position(neighbor, cell_size);

        float edge_weight;

        if (ray_obstacle_intersect(instance, p_current, p_neighbor)) {
          edge_weight = infinityf();
        } else {
          edge_weight = (p_neighbor - p_current).length();
        }

        auto tentative_score = g_current + edge_weight;
        auto key_neighbor = cell_index_key(neighbor);

        float g_neighbor = infinityf();
        if (scores.count(key_neighbor) > 0) {
          g_neighbor = scores.at(key_neighbor).g;
        }

        if (tentative_score < g_neighbor) {
          auto f_neighbor = cost_function(instance, p_neighbor);

          came_from[key_neighbor] = key_current;
          scores[key_neighbor] = {
            tentative_score + f_neighbor,
            tentative_score
          };

          if (in_open_set.count(key_neighbor) == 0) {
            open_set.push(neighbor);
            in_open_set.insert(key_neighbor);
          }
        }
      }
    }
  }

  return result;
}

GROVE_NAMESPACE_END
