#pragma once

#include <vector>
#include <functional>

namespace grove {

struct GridGeometryParams {
public:
  struct Hash {
    std::size_t operator()(const GridGeometryParams& a) const noexcept {
      return std::hash<int>{}(a.num_pts_x) ^ std::hash<int>{}(a.num_pts_z);
    }
  };

public:
  int num_pts_x{};
  int num_pts_z{};

public:
  friend inline bool operator==(const GridGeometryParams& a, const GridGeometryParams& b) {
    return a.num_pts_x == b.num_pts_x && a.num_pts_z == b.num_pts_z;
  }

  friend inline bool operator!=(const GridGeometryParams& a, const GridGeometryParams& b) {
    return !(a == b);
  }

  friend inline bool operator<(const GridGeometryParams& a, const GridGeometryParams& b) {
    return std::tie(a.num_pts_x, a.num_pts_z) < std::tie(b.num_pts_x, b.num_pts_z);
  }
};

std::vector<float> make_reflected_grid_indices(int num_pts_x, int num_pts_z);
std::vector<float> make_reflected_grid_indices(const GridGeometryParams& params);
std::vector<uint16_t> triangulate_reflected_grid(int num_pts_x, int num_pts_z);
std::vector<uint16_t> triangulate_reflected_grid(const GridGeometryParams& params);
std::vector<float> apply_triangle_indices(const std::vector<float>& src,
                                          const std::vector<uint16_t>& inds,
                                          bool include_triangle_index,
                                          int pad_vert_size);

}