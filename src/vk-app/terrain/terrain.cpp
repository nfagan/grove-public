#include "terrain.hpp"
#include "heightmap_io.hpp"
#include "grove/common/common.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/intersect.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/gl/debug/debug_draw.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using BorrowedData = HeightMap<float>::BorrowedData;

HeightMap<float, BorrowedData>
make_height_map(float* data, int texture_dim, double interp_extent) {
  BorrowedData height_map_src(data, texture_dim, texture_dim, 1);
  HeightMap<float, BorrowedData> height_map(height_map_src);
  height_map.set_interpolation_extent(interp_extent);
  return height_map;
}

} //  anon

float Terrain::height_nearest_position(const Vec2f& pos) const {
  auto frac = (pos + terrain_dim * 0.5f) / terrain_dim;
  frac.x = clamp(frac.x, 0.0f, 1.0f);
  frac.y = clamp(frac.y, 0.0f, 1.0f);

  auto store_interp = height_map.get_interpolation_extent();
  height_map.set_interpolation_extent(0.0);

  auto height = float(height_map.raw_value_at_normalized_xz(frac.x, frac.y));
  height_map.set_interpolation_extent(store_interp);

  return height;
}

float Terrain::height_at_position(const Vec2f& pos) const {
  auto frac = (pos + terrain_dim * 0.5f) / terrain_dim;
  frac.x = clamp(frac.x, 0.0f, 1.0f);
  frac.y = clamp(frac.y, 0.0f, 1.0f);

  return float(height_map.raw_value_at_normalized_xz(frac.x, frac.y));
}

void Terrain::set_height_map_data(std::unique_ptr<float[]> data) {
  height_map_data = std::move(data);
  height_map = make_height_map(
    height_map_data.get(), texture_dim, height_map_interpolation_extent);
}

void Terrain::clear() {
  std::fill(height_map_data.get(), height_map_data.get() + texture_dim * texture_dim, 0.0f);
}

float Terrain::height_at_pixel(int x, int y) const {
  auto ind = y * texture_dim + x;
  assert(ind >= 0 && ind < texture_dim * texture_dim);
  return height_map_data[ind];
}

void Terrain::initialize() {
  height_map_data = std::make_unique<float[]>(texture_dim * texture_dim);
  height_map = make_height_map(
    height_map_data.get(), texture_dim, height_map_interpolation_extent);
}

bool Terrain::load_height_map(const char* from_file) {
  int dim;
  std::size_t num_elements;
  auto new_height_map_data = grove::load_height_map(from_file, &num_elements, &dim);

  if (!new_height_map_data) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to load height_map from file.", "Terrain");
    return false;

  } else if (dim != texture_dim) {
    GROVE_LOG_ERROR_CAPTURE_META("Loaded height map has incorrect dimensions.", "Terrain");
    return false;
  }

  height_map_data = std::move(new_height_map_data);
  height_map =
    make_height_map(height_map_data.get(), texture_dim, height_map_interpolation_extent);
  return true;
}

void Terrain::save_height_map(const char* to_file) const {
  bool res =
    grove::save_height_map(to_file, height_map_data.get(), texture_dim * texture_dim, texture_dim);

  if (!res) {
    GROVE_LOG_ERROR_CAPTURE_META("Failed to save height_map to file.", "Terrain");
  } else {
    GROVE_LOG_INFO_CAPTURE_META("Saved height map to file.", "Terrain");
  }
}

namespace {

void draw_surface_cube(const Terrain& terrain, const Camera& camera,
                       const Vec2f& frac_p, const Vec3f& scale, const Vec3f& color) {
  auto p_xz = Terrain::fractional_xz_to_world_xz(frac_p);
  Vec3f p{p_xz.x, terrain.height_nearest_position(p_xz), p_xz.y};
  debug::draw_cube(make_translation_scale(p, scale), camera, color);
}

}

void Terrain::draw_circle_of_cubes_on_surface(const Camera& camera, const Vec2f& center,
                                              float radius, int num_cubes, const Vec3f& scale,
                                              const Vec3f& color) const {
  draw_surface_cube(*this, camera, center, scale, color);

  for (int i = 0; i < num_cubes; i++) {
    auto theta = (float(i) / float(num_cubes)) * grove::two_pi();
    auto frac_x = float(radius * std::cos(theta));
    auto frac_z = float(radius * std::sin(theta));
    auto frac_hit = Vec2f(frac_x, frac_z) + center;

    draw_surface_cube(*this, camera, frac_hit, scale, color);
  }
}

bool Terrain::ray_terrain_intersect(const Ray& mouse_ray, Vec2f* frac_hit_pos) {
  Vec4f ground_plane{0.0f, 1.0f, 0.0f, 0.0f};
  float t;

  if (ray_plane_intersect(mouse_ray, ground_plane, &t)) {
    const auto half_terrain = Terrain::terrain_dim / 2.0f;
    const auto hit_pos = mouse_ray(t);
    *frac_hit_pos = (Vec2f(hit_pos.x, hit_pos.z) + half_terrain) / Terrain::terrain_dim;
    return true;

  } else {
    return false;
  }
}

Vec2f Terrain::fractional_xz_to_world_xz(const Vec2f& v) {
  return (v * 2.0f - 1.0f) * Terrain::terrain_dim / 2.0f;
}

GROVE_NAMESPACE_END
