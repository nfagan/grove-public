#pragma once

#include "grove/math/vector.hpp"
#include "grove/visual/HeightMap.hpp"

namespace grove {

struct Ray;
class Camera;

class Terrain {
public:
  static constexpr int texture_dim = 1024;
  static constexpr float terrain_dim = 512.0f;
  static constexpr double height_map_interpolation_extent = 0.05;

public:
  void initialize();
  void save_height_map(const char* to_file) const;
  bool load_height_map(const char* from_file);
  void clear();

  float height_at_position(const Vec2f& pos) const;
  float height_nearest_position(const Vec2f& pos) const;
  float height_nearest_position_xz(const Vec3f& pos) const {
    return height_nearest_position(Vec2f{pos.x, pos.z});
  }

  const std::unique_ptr<float[]>& read_height_map_data() const {
    return height_map_data;
  }

  void draw_circle_of_cubes_on_surface(const Camera& camera, const Vec2f& center, float radius,
                                       int num_cubes, const Vec3f& scale, const Vec3f& color) const;

  static bool ray_terrain_intersect(const Ray& mouse_ray, Vec2f* hit_pos);
  static Vec2f fractional_xz_to_world_xz(const Vec2f& v);

private:
  void set_height_map_data(std::unique_ptr<float[]> data);
  float height_at_pixel(int x, int y) const;

private:
  std::unique_ptr<float[]> height_map_data;
  mutable HeightMap<float, HeightMap<float>::BorrowedData> height_map;
};

}