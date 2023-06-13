#include "CablePathFinder.hpp"
#include "grove/math/intersect.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/math/string_cast.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

GROVE_NAMESPACE_BEGIN

namespace {

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

std::vector<Vec2f> make_smooth_path(const std::vector<Vec2f>& raw_path, int num_points_insert) {
  int64_t num_raw = raw_path.size();

  if (num_raw <= 1) {
    return raw_path;
  }

  std::vector<Vec2f> up_sampled_path;
  up_sampled_path.reserve(num_raw * (num_points_insert + 1));

  for (int64_t i = 0; i < num_raw-1; i++) {
    auto p0 = raw_path[i];
    auto p1 = raw_path[i+1];
    auto v = p1 - p0;

    up_sampled_path.push_back(p0);

    for (int j = 0; j < num_points_insert; j++) {
      auto t = float(j + 1) / float(num_points_insert);
      auto p = p0 + v * t;
      up_sampled_path.push_back(p);
    }
  }

  up_sampled_path.push_back(raw_path.back());
  int64_t num_up_sampled = up_sampled_path.size();

  std::vector<Vec2f> result;
  result.resize(num_up_sampled);

  constexpr int moving_average_size = 2;

  for (int i = 0; i < num_up_sampled; i++) {
    Vec2f sample{0};
    float denom = 0.0f;

    for (int j = -moving_average_size; j <= moving_average_size; j++) {
      auto ind = i + j;
      if (ind < 0 || ind >= num_up_sampled) {
        continue;
      }

      denom += 1.0f;
      sample += up_sampled_path[ind];
    }

    sample /= denom;
    result[i] = sample;
  }

  return result;
}

std::stringstream make_debug_timing_info(const Vec2f& source, const Vec2f& target,
                                         const CablePathResult& path_find_result,
                                         double dur_ms, double dur_end_pts, int end_pt_iters) {
  std::stringstream info;

  info << "Source: " << to_string(source) << std::endl;
  info << "Target: " << to_string(target) << std::endl;
  info << "Straight line dist: " << (target - source).length() << std::endl;

  info << "Computed path in: " << dur_ms << " ms "
       << "(" << path_find_result.computed_in_num_iters << " iterations) "
       << "[" << dur_end_pts << " ms, " << end_pt_iters << " end-pts]" << std::endl;

  return info;
}

} //  anon

CablePathResult CablePathFinder::compute_path(const Vec2f& source, const Vec2f& target) const {
  if (point_obstacle_intersect(obstacles, source) ||
      point_obstacle_intersect(obstacles, target)) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to compute path; end points intersect obstacle.",
                                 "CablePathFinder");
    return {};
  }

  auto t0 = std::chrono::high_resolution_clock::now();

  CablePathFind::Parameters params{};

  CablePathInstanceData instance_data{&obstacles};
  instance_data.source = source;
  instance_data.target = target;

  const int num_points_insert_in_path = 3;

  auto path_find_result = CablePathFind::compute_path(instance_data, params);
  auto smoothed_path_positions =
    make_smooth_path(path_find_result.path_positions, num_points_insert_in_path);

  double dur_end_pts = 0.0;
  int end_pt_iters = 0;

  if (path_find_result.success && !path_find_result.path_positions.empty()) {
    auto t00 = std::chrono::high_resolution_clock::now();

    auto& p0 = path_find_result.path_positions.front();
    auto& p1 = path_find_result.path_positions.back();

    auto from_source = compute_path_end_point(instance_data.source, p0);
    auto to_target = compute_path_end_point(p1, instance_data.target);

    smoothed_path_positions.insert(smoothed_path_positions.begin(),
                                   from_source.begin(), from_source.end());
    std::copy(to_target.begin(), to_target.end(),
              std::back_inserter(smoothed_path_positions));

    auto t11 = std::chrono::high_resolution_clock::now();
    dur_end_pts = std::chrono::duration<double>(t11 - t00).count() * 1e3;
  }

  smoothed_path_positions.insert(smoothed_path_positions.begin(), instance_data.source);
  smoothed_path_positions.push_back(instance_data.target);

  auto t1 = std::chrono::high_resolution_clock::now();
  auto dur_total = std::chrono::duration<double>(t1 - t0);
  double dur_ms = dur_total.count() * 1e3;

  if (dur_ms > 2.0) {
    auto info = make_debug_timing_info(
      source, target, path_find_result, dur_ms, dur_end_pts, end_pt_iters);
    std::cout << info.str() << std::endl;
//    GROVE_LOG_WARNING_CAPTURE_META(info.str().c_str(), "CablePathFinder");
  }

  if (path_find_result.success) {
    path_find_result.path_positions = std::move(smoothed_path_positions);
  } else {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to compute path.", "CablePathFinder");
  }

  return path_find_result;
}

std::vector<Vec2f> CablePathFinder::compute_path_end_point(const Vec2f& p0, const Vec2f& p1) const {
  CablePathFind::Parameters end_pt_params{};
  end_pt_params.grid_cell_size = CablePathFind::end_point_grid_size;

  auto end_pt_dist = (p1 - p0).length();

  if (end_pt_dist > CablePathFind::end_point_grid_size) {
    CablePathInstanceData end_pt_instance{&obstacles};
    end_pt_instance.source = p0;
    end_pt_instance.target = p1;

    auto end_pt_result = CablePathFind::compute_path(end_pt_instance, end_pt_params);

    if (end_pt_result.success) {
      return end_pt_result.path_positions;

    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to compute path to end point.", "CablePathFinder");
    }
  }

  return {};
}

void CablePathFinder::add_obstacles(const Vec3f* positions,
                                    int num_positions,
                                    float radius,
                                    const Vec2f& position_offset) {
  for (int i = 0; i < num_positions; i++) {
    Vec2f pos{positions[i].x, positions[i].z};
    pos += position_offset;

    CablePathObstacle obstacle{pos, radius};
    add_obstacle(obstacle);
  }
}

CablePathFinder::ObstacleID CablePathFinder::add_obstacle(const Vec2f& position, float radius) {
  CablePathObstacle obstacle{position, radius};
  return add_obstacle(obstacle);
}

void CablePathFinder::modify_obstacle(ObstacleID by_id, const Vec2f& position, float radius) {
  auto it = std::find(obstacle_ids.begin(), obstacle_ids.end(), by_id);
  assert(it != obstacle_ids.end());

  if (it != obstacle_ids.end()) {
    auto idx = it - obstacle_ids.begin();
    obstacles[idx].position = position;
    obstacles[idx].radius = radius;
  }
}

CablePathFinder::ObstacleID CablePathFinder::add_obstacle(const CablePathObstacle& obstacle) {
  auto next_id = next_obstacle_id++;
  obstacles.push_back(obstacle);
  obstacle_ids.push_back(next_id);
  return next_id;
}

void CablePathFinder::remove_obstacle(ObstacleID id) {
  auto it = std::find(obstacle_ids.begin(), obstacle_ids.end(), id);
  assert(it != obstacle_ids.end());
  if (it == obstacle_ids.end()) {
    return;
  }

  auto idx = it - obstacle_ids.begin();
  obstacle_ids.erase(it);
  obstacles.erase(obstacles.begin() + idx);
}

GROVE_NAMESPACE_END
