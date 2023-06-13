#include "geometry.hpp"
#include "grove/common/common.hpp"
#include <cassert>

GROVE_NAMESPACE_BEGIN

std::vector<float> make_reflected_grid_indices(int num_pts_x, int num_pts_z) {
  //  Expect odd number of X points, and more than 2.
  assert(num_pts_x > 2 && num_pts_x % 2 == 1);
  const auto x_dim = num_pts_x / 2;

  std::vector<float> grid_indices;

  for (int i = 0; i < num_pts_z; i++) {
    for (int j = 0; j < num_pts_x; j++) {
      auto x_ind = -x_dim + j;
      auto z_ind = i;

      grid_indices.push_back(float(x_ind));
      grid_indices.push_back(float(z_ind));
    }
  }

  return grid_indices;
}

std::vector<float> make_reflected_grid_indices(const GridGeometryParams& params) {
  return make_reflected_grid_indices(params.num_pts_x, params.num_pts_z);
}

std::vector<uint16_t> triangulate_reflected_grid(int num_pts_x, int num_pts_z) {
  std::vector<uint16_t> result;
  auto x_dim = num_pts_x / 2;

  for (int i = 0; i < num_pts_z-1; i++) {
    for (int j = 0; j < num_pts_x-1; j++) {
      auto x_ind = -x_dim + j;

      //  Outer.
      auto top_right = uint16_t(i * num_pts_x + j);
      auto bottom_right = uint16_t((i+1) * num_pts_x + j);

      //  Inner.
      auto top_left = uint16_t(top_right + 1);
      auto bottom_left = uint16_t(bottom_right + 1);

      if (x_ind >= 0) {
        //  Positive half.
        result.push_back(top_left);
        result.push_back(bottom_left);
        result.push_back(bottom_right);

        result.push_back(bottom_right);
        result.push_back(top_right);
        result.push_back(top_left);
      } else {
        //  Negative half.
        result.push_back(bottom_left);
        result.push_back(bottom_right);
        result.push_back(top_right);

        result.push_back(bottom_left);
        result.push_back(top_right);
        result.push_back(top_left);
      }
    }
  }

  return result;
}

std::vector<uint16_t> triangulate_reflected_grid(const GridGeometryParams& params) {
  return triangulate_reflected_grid(params.num_pts_x, params.num_pts_z);
}

std::vector<float> apply_triangle_indices(const std::vector<float>& src,
                                          const std::vector<uint16_t>& inds,
                                          bool include_triangle_index,
                                          int pad_vert_size) {
  std::vector<float> res;
  int vert_ind{};
  for (uint16_t ind : inds) {
    auto beg = ind * 2;
    for (uint16_t i = 0; i < 2; i++) {
      res.push_back(src[i + beg]);
    }
    if (include_triangle_index) {
      res.push_back(float(int(vert_ind) / 3));
    }
    for (int j = 0; j < pad_vert_size; j++) {
      res.push_back(0.0f);
    }
    vert_ind++;
  }
  return res;
}

GROVE_NAMESPACE_END
