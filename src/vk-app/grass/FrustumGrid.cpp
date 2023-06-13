#include "FrustumGrid.hpp"
#include "grove/common/common.hpp"
#include "grove/common/config.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/profile.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/visual/Camera.hpp"
#include <cmath>

GROVE_NAMESPACE_BEGIN

#define GROVE_CHECK_MIN(component) \
  float component = n0.component; \
  if (n1.component < component) { \
    component = n1.component; \
  } \
  if (f0.component < component) { \
    component = f0.component; \
  } \
  if (f1.component < component) { \
    component = f1.component; \
  } \

#define GROVE_CHECK_MAX(component) \
  float component = n0.component; \
  if (n1.component > component) { \
    component = n1.component; \
  } \
  if (f0.component > component) { \
    component = f0.component; \
  } \
  if (f1.component > component) { \
    component = f1.component; \
  } \

namespace {
  const Vec2f* check_left_most_candidate(const Vec2f* candidate, const Vec2f* query_point) {
    //  Prefer lower left (in z dimension) when x-coords are the same.
    if (query_point->x < candidate->x ||
        (query_point->x == candidate->x && query_point->y < candidate->y)) {
      return query_point;
    } else {
      return candidate;
    }
  }

#if 0
  inline uint64_t pack_int32_t(const int32_t a, const int32_t b) {
    uint64_t packed = 0;
    
    std::memcpy(&packed, &a, sizeof(int32_t));
    std::memcpy(&packed + sizeof(int32_t), &b, sizeof(int32_t));
    
    return packed;
  }
#endif
  
  inline bool sat_intersects(float cx0, float cx1, float cz0, float cz1,
                             float nx, float nz, const Vec4f& trap_corner) {
    const float proj00 = cx0 * nx + cz0 * nz;
    const float proj01 = cx0 * nx + cz1 * nz;
    const float proj10 = cx1 * nx + cz0 * nz;
    const float proj11 = cx1 * nx + cz1 * nz;
    
    //  Min cell
    float min_cell = proj00;
    
    if (proj01 < min_cell) {
      min_cell = proj01;
    }
    if (proj10 < min_cell) {
      min_cell = proj10;
    }
    if (proj11 < min_cell) {
      min_cell = proj11;
    }
    
    //  Max cell
    float max_cell = proj11;
    
    if (proj00 > max_cell) {
      max_cell = proj00;
    }
    if (proj01 > max_cell) {
      max_cell = proj01;
    }
    if (proj10 > max_cell) {
      max_cell = proj10;
    }
    
    //  Min trap
    float min_trap = trap_corner.x;
    
    if (trap_corner.y < min_trap) {
      min_trap = trap_corner.y;
    }
    if (trap_corner.z < min_trap) {
      min_trap = trap_corner.z;
    }
    if (trap_corner.w < min_trap) {
      min_trap = trap_corner.w;
    }
    
    //  Max trap
    float max_trap = trap_corner.x;
    
    if (trap_corner.y > max_trap) {
      max_trap = trap_corner.y;
    }
    if (trap_corner.z > max_trap) {
      max_trap = trap_corner.z;
    }
    if (trap_corner.w > max_trap) {
      max_trap = trap_corner.w;
    }
    
    return !(max_cell < min_trap || min_cell > max_trap);
  }
}

FrustumGrid::Parameters::Parameters() :
  near_scale(0.0f),
  far_scale(0.0f),
  z_extent(0.0f),
  z_offset(0.0f),
  cell_size(0.0f),
  num_cells(0),
  data_size(0),
  alpha_rise_factor(0.1f),
  alpha_decay_factor(0.1f),
  snap_on_rotate(true),
  mark_available_if_behind_camera(true) {
  //
}

FrustumGrid::Parameters::Parameters(const FrustumGrid::MatchCameraParameters& params) : Parameters() {
  const float tan_fov = std::tan(params.field_of_view / 2.0f);
  const auto ar = params.aspect_ratio;
  
  near_scale = tan_fov * ar * (params.z_offset + ar) * 2.0f;
  far_scale = tan_fov * ar * (params.z_offset + params.z_extent + ar) * 2.0f;
  
  num_cells = params.num_cells;
  
  if (params.custom_data_size) {
    data_size = params.data_size;
  } else {
    data_size = grove::next_pow2(num_cells);
  }
  
  z_offset = params.z_offset;
  z_extent = params.z_extent;
}

FrustumGrid::FrustumGrid() : FrustumGrid(Parameters()) {
  //
}

FrustumGrid::FrustumGrid(const Parameters& params) :
  near_scale(params.near_scale),
  far_scale(params.far_scale),
  z_extent(params.z_extent),
  z_offset(params.z_offset),
  cell_size(params.cell_size),
  num_cells(params.num_cells),
  data_size(params.data_size),
  alpha_rise_factor(params.alpha_rise_factor),
  alpha_decay_factor(params.alpha_decay_factor),
  snap_on_rotate(params.snap_on_rotate),
  mark_available_if_behind_camera(params.mark_available_if_behind_camera),
  camera_position(0.0f),
  last_theta(0.0f) {
  //
  if (num_cells > data_size) {
    data_size = num_cells;
  }
    
  initialize();
}

void FrustumGrid::make_corners() {
  corners.f0.y = z_offset + z_extent;
  
  corners.f1.x = far_scale;
  corners.f1.y = z_offset + z_extent;
  
  const float amount_offset = (far_scale - near_scale) / 2.0f;
  
  corners.n0.x = amount_offset;
  corners.n0.y = z_offset;
  
  corners.n1.x = amount_offset + near_scale;
  corners.n1.y = z_offset;
}

void FrustumGrid::make_grid_data() {
  if (num_cells == 0) {
    return;
  }
  
  current_used_indices.resize(num_cells);
  decaying_indices.resize(num_cells);
  
  std::fill(current_used_indices.begin(), current_used_indices.end(), int8_t(-1));
  std::fill(decaying_indices.begin(), decaying_indices.end(), int8_t(-1));
  
  cell_data.resize(num_cells * num_components_per_cell);
  std::fill(cell_data.begin(), cell_data.end(), 0.0f);
  
  for (int32_t i = 0; i < num_cells; i++) {
    available_indices.insert(i);
  }
}

void FrustumGrid::initialize() {
  make_corners();
  make_grid_data();
}

void FrustumGrid::update_normals() {
  make_normal(normals.x0, corners.n0, corners.f0);
  make_normal(normals.x1, corners.f1, corners.n1);
  make_normal(normals.z, corners.n1, corners.n0);
}

void FrustumGrid::make_normal(Vec2f& out, const Vec2f& a, const Vec2f& b) const {
  out = normalize(a - b);
  
  float tmp = out.x;
  out.x = out.y;
  out.y = -tmp;
}

const Vec2f& FrustumGrid::left_most_point() const {
  const Vec2f* candidate = &corners.n0;
  
  candidate = check_left_most_candidate(candidate, &corners.n1);
  candidate = check_left_most_candidate(candidate, &corners.f0);
  candidate = check_left_most_candidate(candidate, &corners.f1);
  
  return *candidate;
}

bool FrustumGrid::set_position_rotation(float x, float z, float theta) {
  if (!std::isfinite(theta)) {
    GROVE_LOG_WARNING_CAPTURE_META("Theta was non-finite.", "frustum-grid");
    return false;
  }
  
  const float amount_offset = (far_scale - near_scale) / 2.0f;
  
  const float ct = std::cos(theta);
  const float st = std::sin(theta);
  
  const float f0x = -far_scale / 2.0f;
  const float f0z = z_extent + z_offset;
  
  const float f1x = far_scale / 2.0f;
  const float f1z = f0z;
  
  const float n0x = -far_scale / 2.0f + amount_offset;
  const float n0z = z_offset;
  
  const float n1x = far_scale / 2.0f - amount_offset;
  const float n1z = z_offset;
  
  corners.f0.x = x + (f0x * ct - f0z * st);
  corners.f0.y = z + (f0z * ct + f0x * st);
  
  corners.f1.x = x + (f1x * ct - f1z * st);
  corners.f1.y = z + (f1z * ct + f1x * st);
  
  corners.n0.x = x + (n0x * ct - n0z * st);
  corners.n0.y = z + (n0z * ct + n0x * st);
  
  corners.n1.x = x + (n1x * ct - n1z * st);
  corners.n1.y = z + (n1z * ct + n1x * st);

  camera_position.x = x;
  camera_position.y = z;

  return true;
}

FrustumGrid::FrustumCornerProjections FrustumGrid::corner_projections() const {
  FrustumGrid::FrustumCornerProjections projections;
  
  projections.n0.x = dot(corners.n0, normals.z);
  projections.n0.y = dot(corners.n1, normals.z);
  projections.n0.z = dot(corners.f1, normals.z);
  projections.n0.w = dot(corners.f0, normals.z);

  projections.n1.x = dot(corners.n0, -normals.z);
  projections.n1.y = dot(corners.n1, -normals.z);
  projections.n1.z = dot(corners.f1, -normals.z);
  projections.n1.w = dot(corners.f0, -normals.z);

  projections.f0.x = dot(corners.n0, normals.x0);
  projections.f0.y = dot(corners.n1, normals.x0);
  projections.f0.z = dot(corners.f1, normals.x0);
  projections.f0.w = dot(corners.f0, normals.x0);

  projections.f1.x = dot(corners.n0, normals.x1);
  projections.f1.y = dot(corners.n1, normals.x1);
  projections.f1.z = dot(corners.f1, normals.x1);
  projections.f1.w = dot(corners.f0, normals.x1);
  
  return projections;
}

int32_t FrustumGrid::get_filled_with(int32_t ix, int32_t iz) const {
  const auto it = in_use_sub_to_ind.find(std::make_pair(ix, iz));
  
  if (it == in_use_sub_to_ind.end()) {
    return -1;
  } else {
    return it->second;
  }
}

void FrustumGrid::mark_filled_with(int32_t ix, int32_t iz, int32_t index) {
  const auto to_hash = std::make_pair(ix, iz);
  in_use_sub_to_ind[to_hash] = index;
}

void FrustumGrid::unmark_filled(int32_t ix, int32_t iz) {
  const auto filled_hash = std::make_pair(ix, iz);
  in_use_sub_to_ind.erase(filled_hash);
}

void FrustumGrid::start_using_cell(int32_t cell_index, int32_t ix, int32_t iz, float alpha0) {
  current_used_indices[cell_index] = 1;

  mark_filled_with(ix, iz, cell_index);

  cell_data[cell_index * num_components_per_cell] = float(ix);
  cell_data[cell_index * num_components_per_cell + 1] = float(iz);
  cell_data[cell_index * num_components_per_cell + 2] = 1.0f;
  cell_data[cell_index * num_components_per_cell + 3] = alpha0;
}

void FrustumGrid::update(const grove::Camera& camera) {
  const Vec3f& front_xz = -camera.get_front_xz();
  const Vec3f& position = camera.get_position();
  const float theta = std::atan2(front_xz.z, front_xz.x) + float(grove::pi()) / 2.0f;
  
  update(position.x, position.z, theta);
}

void FrustumGrid::update(const grove::Camera& camera,
                         float follow_distance,
                         const Vec3f& player_position) {
  const Vec3f& front_xz = -camera.get_front_xz();
  const float theta = std::atan2(front_xz.z, front_xz.x) + float(grove::pi()) / 2.0f;
  const Vec3f position = front_xz * follow_distance + player_position;

  update(position.x, position.z, theta);
}

void FrustumGrid::update(float x, float z, float theta) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("FrustumGrid/update");

  if (!set_position_rotation(x, z, theta)) {
    return;
  }

  update_normals();
  
  const bool is_same_rotation = std::abs(last_theta - theta) < grove::feps();
  last_theta = theta;
  
  const float alpha_rise = (is_same_rotation || !snap_on_rotate) ? alpha_rise_factor : 1.0f;
  const float alpha_decay = (is_same_rotation || !snap_on_rotate) ? alpha_decay_factor : 1.0f;
  
  const Vec2f& left_most = left_most_point();
  const int32_t ix_left = cell_index_x(left_most.x);
  const int32_t iz_left = cell_index_z(left_most.y);
  
  const int32_t imax_z = cell_index_z(corners.max_z());
  const int32_t imin_z = cell_index_z(corners.min_z());
  const int32_t imax_x = cell_index_x(corners.max_x());
  
  const auto projections = corner_projections();
  
  std::fill(current_used_indices.begin(), current_used_indices.end(), int8_t(-1));
  try_to_fill_indices.clear();
  
  for (int i = 0; i < 2; i++) {
    const int32_t z_direction = i == 0 ? 1 : -1;
    
    int32_t iz = iz_left;
    int32_t ix_last = ix_left;
    
    while (iz <= imax_z && iz >= imin_z) {
      bool found_left_edge = false;
      int32_t ix = ix_last;
      
      while (ix <= imax_x) {
        const float x0 = float(ix) * cell_size.x;
        const float z0 = float(iz) * cell_size.y;
        const float x1 = x0 + cell_size.x;
        const float z1 = z0 + cell_size.y;
        
        bool intersects =
          sat_intersects(x0, x1, z0, z1, normals.z.x, normals.z.y, projections.n0);
        intersects =
          intersects && sat_intersects(x0, x1, z0, z1, -normals.z.x, -normals.z.y, projections.n1);
        intersects =
          intersects && sat_intersects(x0, x1, z0, z1, normals.x0.x, normals.x0.y, projections.f0);
        intersects =
          intersects && sat_intersects(x0, x1, z0, z1, normals.x1.x, normals.x1.y, projections.f1);

        if (intersects) {
          const int32_t filled_ind = get_filled_with(ix, iz);
          
          if (filled_ind == -1) {
            try_to_fill_indices.emplace(ix, iz);
          } else {
            //  This cell is / was already filled.
            current_used_indices[filled_ind] = 1;
          }
          
          if (!found_left_edge) {
            ix_last = ix;
            found_left_edge = true;
          }
        } else {
          if (found_left_edge) {
            break;
          }
        }

        ix++;
      }
      
      iz += z_direction;
    }
  }
  
  update_in_use(alpha_decay, alpha_rise);
}

void FrustumGrid::update_in_use(float alpha_decay, float alpha_rise) {
  int num_in_use = 0;
  const Vec2f mid_far = corners.mid_far();
  const Vec2f camera_forwards = normalize(mid_far - camera_position);

  for (int i = 0; i < num_cells; i++) {
    const float ix = cell_data[i * num_components_per_cell];
    const float iz = cell_data[i * num_components_per_cell+1];
    const auto ixi = int32_t(ix);
    const auto izi = int32_t(iz);

    const Vec2f to_camera = normalize(Vec2f(ix, iz) * cell_size - camera_position);

    if (mark_available_if_behind_camera && dot(to_camera, camera_forwards) < 0.0f) {
      //  This cell is behind the camera, so free it.
      const int32_t current_ind = get_filled_with(ixi, izi);

      if (current_ind != -1) {
        unmark_filled(ixi, izi);
        available_indices.insert(current_ind);
        cell_data[current_ind * num_components_per_cell+3] = 0.0f;
      }

    } else if (current_used_indices[i] < 0) {
      float& alpha = cell_data[i * num_components_per_cell+3];
      alpha -= alpha_decay;

      if (alpha <= 0.0f) {
        cell_data[i * num_components_per_cell+2] = 0.0f;
        alpha = 0.0f;

        if (get_filled_with(ixi, izi) == i) {
          unmark_filled(ixi, izi);
          available_indices.insert(i);
        }
      }
    } else {
      assert(available_indices.count(i) == 0 && get_filled_with(ixi, izi) == i);
      num_in_use++;

      float& curr_alpha = cell_data[i * num_components_per_cell+3];
      if (curr_alpha < 1.0f) {
        curr_alpha += alpha_rise;
      }
      if (curr_alpha > 1.0f) {
        curr_alpha = 1.0f;
      }
    }
  }

  auto available_indices_it = available_indices.begin();
  for (auto& to_fill : try_to_fill_indices) {
    if (!available_indices.empty()) {
      auto [ix, iz] = to_fill;

      int32_t free_ind = *available_indices_it;
      available_indices_it = available_indices.erase(available_indices_it);

      start_using_cell(free_ind, ix, iz, 1.0f);
    } else {
      break;
    }
  }

#ifdef GROVE_DEBUG
  for (const auto& it : in_use_sub_to_ind) {
    const int32_t ind = it.second;
    assert(ind >= 0 && ind < num_cells);
    const auto ix_test = int32_t(cell_data[ind * num_components_per_cell]);
    const auto iz_test = int32_t(cell_data[ind * num_components_per_cell+1]);
    assert(ix_test == it.first.first && iz_test == it.first.second);
    assert(available_indices.count(ind) == 0);
  }

  assert(int(in_use_sub_to_ind.size() + available_indices.size()) == num_cells);
#endif

#ifdef GROVE_DEBUG
  if (available_indices.empty()) {
#if 0
    GROVE_LOG_WARNING_CAPTURE_META("All cells in use.", "frustum-grid");
#endif
  }
#endif
}

float FrustumGrid::FrustumCorners::min_x() const {
  GROVE_CHECK_MIN(x)
  return x;
}

float FrustumGrid::FrustumCorners::min_z() const {
  GROVE_CHECK_MIN(y)
  return y;
}

float FrustumGrid::FrustumCorners::max_x() const {
  GROVE_CHECK_MAX(x)
  return x;
}

float FrustumGrid::FrustumCorners::max_z() const {
  GROVE_CHECK_MAX(y)
  return y;
}

Vec2f FrustumGrid::FrustumCorners::mid_far() const {
  return Vec2f((f0.x + f1.x) / 2.0f, (f0.y + f1.y) / 2.0f);
}

int FrustumGrid::get_data_size() const {
  return data_size;
}

int FrustumGrid::get_num_cells() const {
  return num_cells;
}

const Vec2f& FrustumGrid::get_cell_size() const {
  return cell_size;
}

const std::vector<float>& FrustumGrid::get_data() const {
  return cell_data;
}

float FrustumGrid::get_z_extent() const {
  return z_extent;
}

float FrustumGrid::get_z_offset() const {
  return z_offset;
}

GROVE_NAMESPACE_END
