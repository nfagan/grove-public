#pragma once

#include "grove/math/vector.hpp"
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace grove {
  class FrustumGrid;
  class Camera;
}

class grove::FrustumGrid {
public:
  struct MatchCameraParameters;
  
  struct Parameters {
    float near_scale;
    float far_scale;
    float z_extent;
    float z_offset;
    Vec2f cell_size;
    
    int num_cells;
    int data_size;
    
    float alpha_rise_factor;
    float alpha_decay_factor;
    bool snap_on_rotate;
    bool mark_available_if_behind_camera;
    
    Parameters();
    Parameters(const MatchCameraParameters& params);
  };
  
  struct MatchCameraParameters {
    float z_offset;
    float z_extent;
    float field_of_view;
    float aspect_ratio;
    int num_cells;
    int data_size;
    bool custom_data_size;
    
    MatchCameraParameters() :
      z_offset(0.0f),
      z_extent(0.0f),
      field_of_view(0.0f),
      aspect_ratio(0.0f),
      num_cells(0),
      data_size(0),
      custom_data_size(false) {
      //
    }
  };
  
private:
  struct FrustumCorners {
    Vec2f n0;
    Vec2f n1;
    Vec2f f0;
    Vec2f f1;
    
    FrustumCorners() : n0(0.0f), n1(0.0f), f0(0.0f), f1(0.0f) {
      //
    }
    
    float min_x() const;
    float min_z() const;
    float max_x() const;
    float max_z() const;

    Vec2f mid_far() const;
  };
  
  struct FrustumNormals {
    Vec2f x0;
    Vec2f x1;
    Vec2f z;
    
    FrustumNormals() : x0(-1.0f, 0.0f), x1(1.0f, 0.0f), z(0.0f, 1.0f) {
      //
    }
  };
  
  struct FrustumCornerProjections {
    Vec4f n0;
    Vec4f n1;
    Vec4f f0;
    Vec4f f1;
  };
  
public:
  FrustumGrid();
  explicit FrustumGrid(const Parameters& params);
  
  void update(float x, float z, float theta);
  void update(const Camera& camera);
  void update(const Camera& camera, float follow_distance, const Vec3f& player_position);
  
  int get_data_size() const;
  int get_num_cells() const;
  float get_z_extent() const;
  float get_z_offset() const;
  const Vec2f& get_cell_size() const;

  const std::vector<float>& get_data() const;
  int num_available_cells() const {
    return int(available_indices.size());
  }
  
private:
  void initialize();
  
  void make_corners();
  void make_grid_data();
  bool set_position_rotation(float x, float z, float theta);
  
  void update_normals();
  void make_normal(Vec2f& out, const Vec2f& a, const Vec2f& b) const;
  void update_in_use(float alpha_decay, float alpha_rise);
  
  const Vec2f& left_most_point() const;
  
  inline int32_t cell_index_x(float component) const {
    return int32_t(std::floor(component / cell_size.x));
  }
  
  inline int32_t cell_index_z(float component) const {
    return int32_t(std::floor(component / cell_size.y));
  }
  
  FrustumCornerProjections corner_projections() const;
  
  int32_t get_filled_with(int32_t ix, int32_t iz) const;
  void mark_filled_with(int32_t ix, int32_t iz, int32_t index);
  void unmark_filled(int32_t ix, int32_t iz);
  void start_using_cell(int32_t cell_index, int32_t ix, int32_t iz, float initial_alpha);
  
private:
  using PairIndices = std::pair<int32_t, int32_t>;

  struct HashPair {
    size_t operator()(const PairIndices& a) const {
      const auto hash1 = std::hash<int32_t>{}(a.first);
      const auto hash2 = std::hash<int32_t>{}(a.second);
      return hash1 ^ hash2;
    }
  };
  
  std::vector<float> cell_data;
  
  std::set<int32_t> available_indices;
  std::vector<int8_t> current_used_indices;
  std::vector<int8_t> decaying_indices;
  std::unordered_set<PairIndices, HashPair> try_to_fill_indices;
  
  std::unordered_map<PairIndices, int32_t, HashPair> in_use_sub_to_ind;
  
  float near_scale;
  float far_scale;
  
  float z_extent;
  float z_offset;
  
  Vec2f cell_size;
  
  int num_cells;
  int data_size;
  
  float alpha_rise_factor;
  float alpha_decay_factor;
  
  bool snap_on_rotate;
  bool mark_available_if_behind_camera;

  Vec2f camera_position;
  float last_theta;
  
  FrustumCorners corners;
  FrustumNormals normals;
  
  static constexpr int num_components_per_cell = 4;
};
