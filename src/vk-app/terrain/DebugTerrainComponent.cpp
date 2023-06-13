#include "DebugTerrainComponent.hpp"
#include "cube_march.hpp"
#include "place_on_mesh.hpp"
#include "../render/debug_draw.hpp"
#include "../cloud/distribute_points.hpp"
#include "../imgui/TerrainGUI.hpp"
#include "../procedural_tree/serialize_generic.hpp"
#include "../procedural_tree/fit_bounds.hpp"
#include "../procedural_tree/render.hpp"
#include "../procedural_tree/components.hpp"
#include "../terrain/terrain.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/profile.hpp"
#include "grove/load/obj.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Image.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/math/triangle.hpp"
#include "grove/math/bounds.hpp"
#include "grove/math/GridIterator3.hpp"
#include "grove/math/frame.hpp"
#include <unordered_map>
#include <unordered_set>
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateInfo = DebugTerrainComponent::UpdateInfo;
using Model = DebugTerrainComponent::Model;
using AddTransformEditor = DebugTerrainComponent::AddTransformEditor;
using CubeMarchVertex = TerrainRenderer::CubeMarchVertex;

using U163 = Vec3<uint16_t>;

struct HashU163 {
  std::size_t operator()(const U163& key) const noexcept {
    uint64_t v{};
    v |= key.x;
    v |= (uint64_t(key.y) << 16u);
    v |= (uint64_t(key.z) << 32u);
    return std::hash<uint64_t>{}(v);
  }
};

template <typename Value>
struct GridCellCache {
public:
  void insert(const U163& key, Value value) {
    cache[key] = value;
  }
  void erase(const U163& key) {
    cache.erase(key);
  }
  Value* find(const U163& key) {
    auto it = cache.find(key);
    return it == cache.end() ? nullptr : &it->second;
  }
  void clear() {
    cache.clear();
  }

public:
  std::unordered_map<U163, Value, HashU163> cache;
};

using ChunkIndices = std::unordered_set<U163, HashU163>;

struct VoxelSamples {
  int block_size() const {
    return cache_block_dim * cache_block_dim * cache_block_dim;
  }

  int to_local_offset(const U163& off) const {
    auto slab_off = int(cache_block_dim * cache_block_dim * off.z);
    auto im_off = int(cache_block_dim * off.y + off.x);
    auto local_off = im_off + slab_off;
    assert(local_off < block_size());
    return local_off;
  }

  bool set_if_present(const U163& p, uint8_t val) {
    auto base = p / cache_block_dim;
    const U163 key = base;
    auto it = cache.find(key);
    if (it != cache.end()) {
      const auto off = p - base * cache_block_dim;
      auto tot_off = to_local_offset(off) + it->second;
      assert(tot_off < int(samples.size()));
      if (samples[tot_off] != 0xff) {
        samples[tot_off] = val;
        return true;
      }
    }
    return false;
  }

  void set(const U163& p, uint8_t val) {
    auto base = p / cache_block_dim;
    const U163 key = base;
    auto it = cache.find(key);
    if (it == cache.end()) {
      const auto next = int(samples.size());
      samples.resize(samples.size() + block_size());
      std::fill(samples.begin() + next, samples.end(), (unsigned char) 0xff);
      samples[next] = val;
      cache[key] = next;
    } else {
      const auto off = p - base * cache_block_dim;
      auto tot_off = to_local_offset(off) + it->second;
      assert(tot_off < int(samples.size()));
      samples[tot_off] = val;
    }
  }

  uint8_t sample(const Vec3<uint16_t>& p) const {
    auto base = p / cache_block_dim;
    const U163 key = base;
    auto it = cache.find(key);
    if (it == cache.end()) {
      return 0xff;
    } else {
      const auto off = p - base * cache_block_dim;
      auto tot_off = to_local_offset(off) + it->second;
      assert(tot_off < int(samples.size()));
      return samples[tot_off];
    }
  }

  void clear() {
    cache.clear();
    samples.clear();
  }

  int num_samples() const {
    return int(samples.size());
  }
  int num_blocks() const {
    return num_samples() / int(cache_block_dim);
  }

  uint16_t cache_block_dim{8};
  std::unordered_map<U163, int, HashU163> cache;
  std::vector<uint8_t> samples;
};

struct CubeMarchMeshData {
public:
  static constexpr uint16_t chunk_dim = 8;

  struct Chunk {
    TerrainRenderer::CubeMarchChunkHandle renderer_chunk{};
    std::vector<CubeMarchVertex> vertices;
  };

public:
  int num_chunks() const {
    return int(chunks.cache.size());
  }
  int total_num_vertices() const {
    int sum{};
    for (auto& [_, chunk] : chunks.cache) {
      sum += int(chunk.vertices.size());
    }
    return sum;
  }
  int total_num_triangles() const {
    return total_num_vertices() / 3;
  }
  void clear(TerrainRenderer& renderer) {
    for (auto& [_, chunk] : chunks.cache) {
      renderer.destroy_chunk(chunk.renderer_chunk);
    }
    chunks.clear();
  }

public:
  GridCellCache<Chunk> chunks;
};

struct OrientedCylinder {
  Vec3f i;
  Vec3f j;
  Vec3f k;
  Vec3f p;
  float r;
  float half_length;
};

[[maybe_unused]] OrientedCylinder obb3_to_inner_cylinder(const OBB3f& obb) {
  OrientedCylinder result;
  result.i = obb.i;
  result.j = obb.j;
  result.k = obb.k;
  result.p = obb.position;
  result.r = std::min(obb.half_size.x, obb.half_size.z);
  result.half_length = obb.half_size.y;
  return result;
}

float sdf_sphere(const Vec3f& qp, const Vec3f& p, float r) {
  return (qp - p).length() - r;
}

float sdf_cylinder(const Vec3f& qp, const Mat3f& frame, const Vec3f& p, float r, float h2) {
  auto tp = transpose(frame) * (qp - p);
  auto tpxz = Vec2f{tp.x, tp.z};
  float dxz = tpxz.length();
  float abs_y = std::abs(tp.y);
  if (abs_y <= h2) {
    float d_xz = dxz - r;
    if (d_xz < 0.0f) {
      return std::max(d_xz, abs_y - h2);
    } else {
      return d_xz;
    }
  } else {
    if (dxz > r) {
      auto dir = tpxz / dxz;
      auto surf_pxz = dir * r;
      Vec3f surf_p{surf_pxz.x, h2, surf_pxz.y};
      return (Vec3f{tp.x, abs_y, tp.z} - surf_p).length();
    } else {
      return abs_y - h2;
    }
  }
}

[[maybe_unused]] float sdf_cylinder(const Vec3f& qp, const OrientedCylinder& c) {
  return sdf_cylinder(qp, Mat3f{c.i, c.j, c.k}, c.p, c.r, c.half_length);
}

template <typename T>
T sdf_obb(const Vec3<T>& qp, const OBB3<T>& obb) {
  auto x0 = obb.position - obb.i * obb.half_size.x;
  auto x1 = obb.position + obb.i * obb.half_size.x;

  auto y0 = obb.position - obb.j * obb.half_size.y;
  auto y1 = obb.position + obb.j * obb.half_size.y;

  auto z0 = obb.position - obb.k * obb.half_size.z;
  auto z1 = obb.position + obb.k * obb.half_size.z;

  auto a = Mat3<T>{obb.i, obb.j, obb.k};
  auto rot_pos = transpose(a);  //  inv(a)
  Vec3<T> rot_ps[6] = {x0, y0, z0, x1, y1, z1};

  T ds[6];
  bool any_outside{};
  for (int i = 0; i < 3; i++) {
    auto v = rot_pos * (qp - rot_ps[i]);
    ds[i] = v[i];
    any_outside |= ds[i] < T(0);
  }

  for (int i = 0; i < 3; i++) {
    auto at = a;
    const Vec3<T> col = at[i];
    at[i] = -col;
    at = transpose(at); //  inv(at)
    auto v = at * (qp - rot_ps[i + 3]);
    ds[i + 3] = v[i];
    any_outside |= ds[i + 3] < T(0);
  }

  if (any_outside) {
    T max_d = std::numeric_limits<T>::lowest();
    for (T d : ds) {
      if (d < T(0)) {
        max_d = std::max(max_d, -d);
      }
    }
    return max_d;
  } else {
    auto elem = *std::min_element(ds, ds + 6);
    return -elem;
  }
}

void distribute_cube_march_vertex_normals(const std::vector<Vec3f>& pos,
                                          std::vector<CubeMarchVertex>& packed_pos) {
  struct HashVec3 {
    std::size_t operator()(const Vec3f& p) const noexcept {
      uint32_t x;
      uint32_t y;
      uint32_t z;
      memcpy(&x, &p.x, sizeof(float));
      memcpy(&y, &p.y, sizeof(float));
      memcpy(&z, &p.z, sizeof(float));
      return x ^ y ^ z;
    }
  };

  std::unordered_map<Vec3f, std::vector<uint32_t>, HashVec3> hash;
  for (auto& p : pos) {
    if (hash.count(p) == 0) {
      hash[p] = {};
    }
    hash.at(p).push_back(uint32_t(&p - pos.data()));
  }

  std::unordered_map<Vec3f, Vec3f, HashVec3> ns;
  for (auto& [p, pis] : hash) {
    Vec3f n{};
    float ct{};
    for (uint32_t pi : pis) {
      n += packed_pos[pi].normal;
      ct += 1.0f;
    }
    assert(ct > 0.0f);
    n = normalize(n / ct);
    assert(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z));
    ns[p] = n;
  }

  for (auto& v : packed_pos) {
    v.normal = ns.at(v.position);
  }
}

[[maybe_unused]]
std::vector<CubeMarchVertex> to_cube_march_vertices(const std::vector<Vec3f>& ps,
                                                    const std::vector<Vec3f>& ns) {
  assert(ps.size() == ns.size());
  std::vector<CubeMarchVertex> result(ps.size());
  for (size_t i = 0; i < ps.size(); i++) {
    auto& r = result.emplace_back();
    r.position = ps[i];
    r.normal = ns[i];
  }
  return result;
}

[[maybe_unused]]
std::vector<CubeMarchVertex> to_cube_march_vertices_with_normals(
  const std::vector<Vec3f>& pos, const std::vector<Vec3f>& normals) {
  //
  std::vector<uint32_t> ti(pos.size());
  uint32_t t_ind{};
  for (auto& t : ti) {
    t = t_ind++;
  }

  std::vector<CubeMarchVertex> packed_pos(pos.size());
  {
    for (size_t i = 0; i < pos.size()/3; i++) {
      Vec3f ps[3] = {pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]};
      Vec3f ns[3] = {normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
      for (int j = 0; j < 3; j++) {
        auto v = i * 3 + j;
        packed_pos[v].position = ps[j];
        packed_pos[v].normal = ns[j];
      }
    }
  }

#if 0
  distribute_cube_march_vertex_normals(pos, packed_pos);
#endif

  return packed_pos;
}

[[maybe_unused]]
std::vector<CubeMarchVertex> to_cube_march_vertices(const std::vector<Vec3f>& pos) {
  std::vector<uint32_t> ti(pos.size());
  uint32_t t_ind{};
  for (auto& t : ti) {
    t = t_ind++;
  }

  std::vector<CubeMarchVertex> packed_pos(pos.size());
  {
    for (size_t i = 0; i < pos.size()/3; i++) {
      auto& p0 = pos[i * 3];
      auto& p1 = pos[i * 3 + 1];
      auto& p2 = pos[i * 3 + 2];
      Vec3f ps[3] = {p0, p1, p2};
      for (int j = 0; j < 3; j++) {
        auto v = i * 3 + j;
        packed_pos[v].position = ps[j];
      }
    }
  }

  std::vector<uint32_t> cts(pos.size());
  tri::compute_normals(
    ti.data(), uint32_t(ti.size())/3,
    packed_pos.data(),
    packed_pos.data(),
    cts.data(),
    0,
    uint32_t(CubeMarchVertex::stride()), uint32_t(CubeMarchVertex::position_offset()),
    uint32_t(CubeMarchVertex::stride()), uint32_t(CubeMarchVertex::normal_offset()));

#if 1
  distribute_cube_march_vertex_normals(pos, packed_pos);
#endif

  return packed_pos;
}

Bounds3f to_quantized_aabb(const Bounds3f& src, const cm::GridInfo& grid) {
  auto p0_rel = floor((src.min - grid.offset) / grid.scale);
  auto p1_rel = floor((src.max - grid.offset) / grid.scale);
  return Bounds3f{p0_rel, p1_rel};
}

Bounds3f sphere_to_quantized_aabb(const Vec3f& p, float radius, const cm::GridInfo& grid) {
  auto grid_rel = p - grid.offset;
  auto p0 = floor((grid_rel - radius) / grid.scale);
  auto sz = max(Vec3f{1.0f}, floor(Vec3f{radius * 2.0f} / grid.scale));
  return Bounds3f{p0, p0 + sz};
}

Bounds3f to_clamped_padded_aabb(Bounds3f bounds, const cm::GridInfo& grid) {
  bounds.min = clamp_each(bounds.min - 1.0f, Vec3f{}, grid.size - 1.0f);
  bounds.max = clamp_each(bounds.max + 1.0f, Vec3f{}, grid.size - 1.0f);
  return bounds;
}

Vec3<uint16_t> to_u16(const Vec3f& v) {
  assert(floor(v) == v && all(lt(v, Vec3<float>{float(0xffff)})));
  return Vec3<uint16_t>{uint16_t(v.x), uint16_t(v.y), uint16_t(v.z)};
}

template <typename T>
Vec3<int> to_int(const Vec3<T>& v) {
  return Vec3<int>{int(v.x), int(v.y), int(v.z)};
}

void to_span(const Vec3f& p0, const Vec3f& p1, U163* beg, U163* sz) {
  assert(floor(p0) == p0 && floor(p1) == p1 && all(ge(p1, p0)) && all(ge(p0, Vec3f{})));
  *beg = to_u16(p0);
  *sz = to_u16(p1 - p0);
}

auto obb3_to_quantized_span(const OBB3f& bounds, const cm::GridInfo& grid) {
  struct Result {
    U163 p0u;
    U163 szu;
  };
  auto [p0, p1] = to_clamped_padded_aabb(to_quantized_aabb(obb3_to_aabb(bounds), grid), grid);
  Result result{};
  to_span(p0, p1, &result.p0u, &result.szu);
  return result;
}

auto sphere_to_quantized_span(const Vec3f& p, float radius, const cm::GridInfo& grid) {
  struct Result {
    U163 p0u;
    U163 szu;
  };

  auto local_bounds = sphere_to_quantized_aabb(p, radius, grid);
  auto [p0, p1] = to_clamped_padded_aabb(local_bounds, grid);
  Result result{};
  to_span(p0, p1, &result.p0u, &result.szu);
  return result;
}

float max_distance(const cm::GridInfo& grid) {
  return grid.scale.length();
}

Vec3f coord_to_world(const Vec3<uint16_t>& p, const cm::GridInfo& grid) {
  return to_vec3f(p) * grid.scale + grid.offset;
}

uint8_t to_distance(float dist, float max_dist, bool invert) {
  const float sign = (dist >= 0.0f ? 1.0f : -1.0f) * (invert ? -1.0f : 1.0f);
  dist = std::min(std::abs(dist), max_dist) * sign;
  dist = clamp((dist / max_dist) * 0.5f + 0.5f, 0.0f, 1.0f);
  return uint8_t(dist * 255.0f);
}

void adjust_in_sphere(const U163& p0, const U163& sz, const cm::GridInfo& grid,
                      const Vec3f& p, float r, VoxelSamples& samples, bool invert) {
  const float max_dist = max_distance(grid);
  for (auto it = begin_it(p0, p0 + sz); is_valid(it); ++it) {
    auto key = *it;
    uint8_t s = samples.sample(key);

    auto key_p = coord_to_world(key, grid);
    auto key_dist = (key_p - p).length();
    auto new_s = to_distance(key_dist - r, max_dist, invert);
    s = invert ? std::max(s, new_s) : std::min(s, new_s);
    samples.set(key, s);
  }
}

void insert_obb_hole(const U163& p0, const U163& sz, const cm::GridInfo& grid,
                     const OBB3f& bounds, VoxelSamples& samples) {
  const float max_dist = max_distance(grid);
  for (auto it = begin_it(p0, p0 + sz); is_valid(it); ++it) {
    auto key = *it;
    auto key_p = coord_to_world(key, grid);
    auto obb_dist = sdf_obb(key_p, bounds);
    if (obb_dist < 0.0f) {
      samples.set(key, to_distance(obb_dist, max_dist, true));
    }
  }
}

void insert_chunk_indices(ChunkIndices& indices, const U163& p0, const U163& sz, uint16_t dim) {
  for (auto it = begin_it(p0, p0 + sz); is_valid(it); ++it) {
    indices.insert((*it) / dim);
  }
}

void modify_mesh_data(CubeMarchMeshData& mesh_data, const U163& chunk_key,
                      const Bounds3f& chunk_world_bound, std::vector<CubeMarchVertex>&& verts,
                      const UpdateInfo& info) {
  const auto num_verts = uint32_t(verts.size());
  //  require draw buffers for chunk and fill.
  if (mesh_data.chunks.find(chunk_key) == nullptr) {
    CubeMarchMeshData::Chunk mesh_chunk{};
    mesh_data.chunks.insert(chunk_key, std::move(mesh_chunk));
  }

  auto* mesh_chunk = mesh_data.chunks.find(chunk_key);
  assert(mesh_chunk);
  mesh_chunk->vertices = std::move(verts);

  auto get_data = [&mesh_data, chunk_key](const void** ptr, size_t* sz) -> void {
    auto* src_chunk = mesh_data.chunks.find(chunk_key);
    assert(src_chunk);
    *ptr = src_chunk->vertices.data();
    *sz = src_chunk->vertices.size() * CubeMarchVertex::stride();
  };

  auto& renderer = info.terrain_renderer;
  auto& renderer_ctx = info.terrain_renderer_context;
  renderer.require_chunk(
    renderer_ctx, &mesh_chunk->renderer_chunk, num_verts, std::move(get_data), chunk_world_bound);
  renderer.set_chunk_modified(renderer_ctx, mesh_chunk->renderer_chunk);
}

void regen_chunks(const cm::GridInfo& grid, VoxelSamples& samples,
                  const ChunkIndices& chunks, CubeMarchMeshData& mesh_data,
                  const UpdateInfo& info) {
  auto gen_surface = [&](const Vec3f& p) -> float {
    auto c = cm::world_to_coord(p, grid);
    assert(all(ge(c, Vec3f{})));
    assert(all(le(c, grid.size)));
    const uint8_t s = samples.sample(to_u16(c));
    return (float(s) / float(0xff) * 2.0f - 1.0f) * max_distance(grid);
  };

  auto grid_sz = to_u16(grid.size);
  for (const U163 chunk_key : chunks) {
    auto chunk_beg = chunk_key * CubeMarchMeshData::chunk_dim;
    auto chunk_end = chunk_beg + CubeMarchMeshData::chunk_dim;
    assert(all(lt(chunk_beg, grid_sz)));

    chunk_beg = max(U163{1}, chunk_beg);
    chunk_end = min(chunk_end, grid_sz - uint16_t(1));
    auto chunk_p0 = to_int(chunk_beg);
    auto chunk_p1 = to_int(chunk_end);

    cm::GenTrisParams params{};
    params.smooth = true;

    std::vector<Vec3f> ps;
    std::vector<Vec3f> ns;
    cm::simple_grid_march_range(grid, gen_surface, 0.0f, chunk_p0, chunk_p1, params, &ps, &ns);
#if GROVE_CUBE_MARCH_INCLUDE_NORMALS
//    auto verts = to_cube_march_vertices(ps, ns);
    auto verts = to_cube_march_vertices_with_normals(ps, ns);
#else
    auto verts = to_cube_march_vertices(ps);
#endif

    const Bounds3f chunk_aabb{coord_to_world(chunk_beg, grid), coord_to_world(chunk_end, grid)};
    modify_mesh_data(mesh_data, chunk_key, chunk_aabb, std::move(verts), info);
  }
}

void adjust_in_radius(const Vec3f& p, float radius, const cm::GridInfo& grid,
                      VoxelSamples& samples, bool invert, ChunkIndices& chunks) {
  auto [p0u, szu] = sphere_to_quantized_span(p, radius, grid);
  adjust_in_sphere(p0u, szu, grid, p, radius, samples, invert);
  insert_chunk_indices(chunks, p0u, szu, CubeMarchMeshData::chunk_dim);
}

void insert_obb_hole(const OBB3f& bounds, const cm::GridInfo& grid, VoxelSamples& samples,
                     ChunkIndices& chunks) {
  auto [p0u, szu] = obb3_to_quantized_span(bounds, grid);
  insert_obb_hole(p0u, szu, grid, bounds, samples);
  insert_chunk_indices(chunks, p0u, szu, CubeMarchMeshData::chunk_dim);
}

auto moved_sphere_to_quantized_span(const Vec3f& prev_p, float prev_r,
                                    const Vec3f& curr_p, float curr_r,
                                    const cm::GridInfo& grid) {
  auto [prev_p0u, prev_szu] = sphere_to_quantized_span(prev_p, prev_r, grid);
  auto [curr_p0u, curr_szu] = sphere_to_quantized_span(curr_p, curr_r, grid);
  auto p1 = max(prev_p0u + prev_szu, curr_p0u + curr_szu);
  p1 = clamp_each(p1 + U163(1), U163{}, to_u16(grid.size) - uint16_t(1));
  auto p0 = min(prev_p0u, curr_p0u);
  auto sz = p1 - p0;
  return std::make_tuple(p0, sz);
}

void move_sphere(const U163& p0, const U163& sz, const cm::GridInfo& grid, VoxelSamples& samples,
                 const Vec3f* ps, const float* rs, int num_spheres,
                 const OBB3f* obb_holes, int num_obb_holes) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("DebugTerrainComponent/move_sphere");
  (void) profiler;

  const float max_dist = max_distance(grid);
  for (auto it = begin_it(p0, p0 + sz); is_valid(it); ++it) {
    auto key_p = coord_to_world(*it, grid);

    float min_dist = max_dist;
    for (int i = 0; i < num_spheres; i++) {
      float dist = sdf_sphere(key_p, ps[i], rs[i]);
      min_dist = std::min(min_dist, dist);
    }

    float max_hole_dist = -infinityf();
    bool in_hole{};
    for (int i = 0; i < num_obb_holes; i++) {
#if 0
      float dist = sdf_cylinder(key_p, obb3_to_inner_cylinder(obb_holes[i]));
#else
      float dist = sdf_obb(key_p, obb_holes[i]);
#endif
      if (dist < 0.0f) {
        max_hole_dist = std::max(max_hole_dist, dist);
        in_hole = true;
      }
    }

    uint8_t set_dist;
    if (in_hole) {
      set_dist = to_distance(max_hole_dist, max_dist, true);
      samples.set_if_present(*it, set_dist);
    } else {
      set_dist = to_distance(min_dist, max_dist, false);
      samples.set(*it, set_dist);
    }

//    samples.set(*it, set_dist);
  }
}

struct PlaceOnMeshResult {
  std::vector<OBB3f> bounds;
  std::vector<mesh::PlacePointsWithinOBB3Entry> point_entries;
};

PlaceOnMeshResult debug_place_on_mesh(const std::vector<Vec3f>& ps, const std::vector<Vec3f>& ns,
                                      const Vec3f& obb3_size) {
  PlaceOnMeshResult result{};

  std::vector<uint32_t> tis(ps.size());
  std::iota(tis.begin(), tis.end(), 0u);

  const auto num_tris = uint32_t(tis.size() / 3);
  std::vector<Bounds2f> tmp_bounds(num_tris);
  std::vector<float> tmp_depths(num_tris);
  mesh::project_vertices_to_aabbs(
    tis.data(), num_tris,
    ps.data(), uint32_t(ps.size()),
    Vec3f{0.0f, 1.0f, 0.0f},
    tmp_bounds.data(), tmp_depths.data());

  constexpr int im_dim = 128;
  int ti_im[im_dim * im_dim];
  {
    float ti_depth[im_dim * im_dim];
    mesh::rasterize_bounds(
      tmp_bounds.data(),
      tmp_depths.data(),
      int(tmp_bounds.size()), im_dim, im_dim, ti_im, ti_depth);
  }

  constexpr int num_box_ps = 128;
  constexpr int num_place_ps = 8;
  constexpr int num_sample_ps = 100;
  Vec2f sample_ps[num_sample_ps];
  Vec2f place_ps[num_place_ps];
  Vec2f box_ps[num_box_ps];
  {
    bool accept_points[num_sample_ps]{};
    points::place_outside_radius<Vec2f, float, 2>(sample_ps, accept_points, num_sample_ps, 0.07f);
  }
  {
    bool accept_points[num_place_ps]{};
    points::place_outside_radius<Vec2f, float, 2>(place_ps, accept_points, num_place_ps, 0.33f);
  }
  {
    bool accept_points[num_box_ps]{};
    points::place_outside_radius<Vec2f, float, 2>(box_ps, accept_points, num_box_ps, 0.07f);
  }

  std::vector<OBB3f> bounds;
  std::vector<mesh::PlacePointsWithinOBB3Entry> point_entries;
  for (int i = 0; i < num_box_ps; i++) {
    mesh::GenOBB3OriginDirectionParams gen_box_params{};
    gen_box_params.image_sample_center_position = box_ps[i];
    gen_box_params.image_sample_size = Vec2f{0.02f};
    gen_box_params.image_sample_positions = sample_ps;
    gen_box_params.num_samples = num_sample_ps;

    gen_box_params.tris = tis.data();
    gen_box_params.ps = ps.data();
    gen_box_params.ns = ns.data();

    gen_box_params.ti_im = ti_im;
    gen_box_params.ti_im_rows = im_dim;
    gen_box_params.ti_im_cols = im_dim;

    auto box_res = mesh::gen_obb3_origin_direction(gen_box_params);
    if (!box_res.success) {
      continue;
    }

    mesh::PlacePointsWithinOBB3Entry place_result_entries[num_place_ps];

    mesh::PlacePointsWithinOBB3Params place_points_params{};
    place_points_params.tris = tis.data();
    place_points_params.num_tris = num_tris;
    place_points_params.ps = ps.data();

    place_points_params.surface_p = box_res.p;
    place_points_params.obb3_frame = box_res.frame;
    place_points_params.obb3_size = obb3_size;

    place_points_params.sample_positions = place_ps;
    place_points_params.num_samples = num_place_ps;
    place_points_params.result_entries = place_result_entries;

    auto place_hits = mesh::place_points_within_obb3(place_points_params);
    if (place_hits.num_hits > 0) {
      const auto dst_bounds = int(bounds.size());
      bounds.emplace_back() = mesh::gen_obb3(
        box_res.p, box_res.frame, obb3_size, place_hits.min_ray_t, place_hits.max_ray_t);

      for (int j = 0; j < place_hits.num_hits; j++) {
        place_result_entries[j].obb3_index = dst_bounds;
        point_entries.push_back(place_result_entries[j]);
      }
    }
  }

  result.bounds = std::move(bounds);
  result.point_entries = std::move(point_entries);
  return result;
}

Optional<obj::VertexData> load_obj(const std::string& p) {
  bool success{};
  auto res = obj::load_simple(p.c_str(), nullptr, &success);
  if (success) {
    return Optional<obj::VertexData>(std::move(res));
  } else {
    return NullOpt{};
  }
}

Optional<Image<uint8_t>> load_image(const std::string& p) {
  bool success{};
  auto res = grove::load_image(p.c_str(), &success, true);
  if (success) {
    return Optional<Image<uint8_t>>(std::move(res));
  } else {
    return NullOpt{};
  }
}

void update_debug_geometry(DebugTerrainComponent& component, Model& model, const UpdateInfo& info) {
  if (!component.geometry_file_path) {
    return;
  }
  if (auto geom = load_obj(component.geometry_file_path.value())) {
    VertexBufferDescriptor desc;
    desc.add_attribute(AttributeDescriptor::float3(0));
    desc.add_attribute(AttributeDescriptor::float3(1));
    desc.add_attribute(AttributeDescriptor::float2(2));

    auto& vd = geom.value();
    info.model_renderer.require_geometry(
      info.model_renderer_context,
      vd.packed_data.data(),
      desc, vd.packed_data.size() * sizeof(float), 0, 1, 2, &model.geom);
  }
  component.geometry_file_path = NullOpt{};
}

void update_debug_image(DebugTerrainComponent& component, Model& model, const UpdateInfo& info) {
  if (!component.image_file_path) {
    return;
  }
  if (auto maybe_im = load_image(component.image_file_path.value())) {
    auto& im = maybe_im.value();
    if (im.num_components_per_pixel == 4) {
      vk::SampledImageManager::ImageCreateInfo im_info{};
      im_info.data = im.data.get();
      im_info.descriptor = image::Descriptor::make_2d_uint8n(im.width, im.height, 4);
      im_info.format = VK_FORMAT_R8G8B8A8_SRGB;
      im_info.image_type = vk::SampledImageManager::ImageType::Image2D;
      im_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
      info.sampled_image_manager.require_sync(&model.image, im_info);

      if (model.image.is_valid() && !model.material.is_valid()) {
        auto maybe_mat = info.model_renderer.add_texture_material(
          info.model_renderer_context, model.image);
        if (maybe_mat) {
          model.material = maybe_mat.value();
        }
      }
    }
  }
  component.image_file_path = NullOpt{};
}

Optional<vk::SampledImageManager::Handle>
update_splotch_image(DebugTerrainComponent& component, const UpdateInfo& info) {
  if (!component.splotch_image_file_path) {
    return NullOpt{};
  }

  auto im_p = std::move(component.splotch_image_file_path.value());
  component.splotch_image_file_path = NullOpt{};

  if (auto maybe_im = load_image(im_p)) {
    auto& im = maybe_im.value();
    if (im.num_components_per_pixel == 1) {
      vk::SampledImageManager::ImageCreateInfo im_info{};
      im_info.data = im.data.get();
      im_info.descriptor = image::Descriptor::make_2d_uint8n(im.width, im.height, 1);
      im_info.format = VK_FORMAT_R8_UNORM;
      im_info.image_type = vk::SampledImageManager::ImageType::Image2D;
      im_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
      info.sampled_image_manager.require_sync(&component.splotch_image, im_info);
      return Optional<vk::SampledImageManager::Handle>(component.splotch_image);
    }
  }
  return NullOpt{};
}

Optional<vk::SampledImageManager::Handle>
update_ground_color_image(DebugTerrainComponent& component, const UpdateInfo& info) {
  if (!component.color_image_file_path) {
    return NullOpt{};
  }

  auto im_p = std::move(component.color_image_file_path.value());
  component.color_image_file_path = NullOpt{};

  if (auto maybe_im = load_image(im_p)) {
    auto& im = maybe_im.value();
    if (im.num_components_per_pixel == 4) {
      vk::SampledImageManager::ImageCreateInfo im_info{};
      im_info.data = im.data.get();
      im_info.descriptor = image::Descriptor::make_2d_uint8n(im.width, im.height, 4);
      im_info.format = VK_FORMAT_R8G8B8A8_SRGB;
      im_info.image_type = vk::SampledImageManager::ImageType::Image2D;
      im_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
      info.sampled_image_manager.require_sync(&component.ground_color_image, im_info);
      return Optional<vk::SampledImageManager::Handle>(component.ground_color_image);
    }
  }
  return NullOpt{};
}

void update_debug_drawable(Model& model, transform::TransformInstance* tform,
                           const UpdateInfo& info) {
  if (!model.drawable.is_valid() && model.geom.is_valid() && model.material.is_valid()) {
    StaticModelRenderer::DrawableParams draw_params{};
    draw_params.transform = Mat4f(1.0f);

    auto handle = info.model_renderer.add_drawable(
      info.model_renderer_context, model.geom, model.material, draw_params);
    if (handle) {
      model.drawable = handle.value();
    }
  }

  if (model.drawable.is_valid() && tform) {
    StaticModelRenderer::DrawableParams draw_params{};
    draw_params.transform = to_mat4(tform->get_current());
    info.model_renderer.set_params(model.drawable, draw_params);
  }
}

cm::GridInfo define_grid() {
  constexpr Vec3f grid_dim = Vec3f{256.0f, 128.0f, 256.0f};
  constexpr Vec3f grid_scl = Vec3f{2.0f};
  cm::GridInfo grid{};
  grid.offset = -grid_dim * 0.5f * grid_scl;
  grid.size = grid_dim;
  grid.scale = grid_scl;
  return grid;
}

Optional<tree::Internodes> read_root_internodes() {
  auto p = std::string{GROVE_ASSET_DIR} + "/serialized_roots/eg8.dat";
  if (auto nodes = tree::io::deserialize(p)) {
    tree::Internodes result;
    for (auto& node : nodes.value()) {
      auto& inode = result.emplace_back();
      inode.parent = node.parent;
      inode.medial_child = node.medial_child;
      inode.lateral_child = node.lateral_child;
      inode.position = node.position;
      inode.render_position = node.position;
      inode.direction = node.direction;
      inode.length = node.length;
      inode.diameter = node.diameter;
    }
    if (!result.empty()) {
      auto root_p = result[0].position;
      for (auto& node : result) {
        node.translate(-root_p);
      }
    }
    return Optional<tree::Internodes>(std::move(result));
  } else {
    return NullOpt{};
  }
}

void to_roots_instances(const tree::Internode* inodes, int num_inodes,
                        ProceduralTreeRootsRenderer::Instance* dst) {
  for (int i = 0; i < num_inodes; i++) {
    auto& node = inodes[i];
    auto bounds = tree::internode_obb(node);
    auto child_bounds = bounds;
    auto child_pos = node.tip_position();
    auto child_radius = 0.0025f;

    const tree::Internode* child{};
    if (node.has_medial_child()) {
      child = inodes + node.medial_child;
    } else if (node.has_lateral_child()) {
      child = inodes + node.lateral_child;
    }

    if (child) {
      child_bounds = tree::internode_obb(*child);
      child_pos = child->position;
      child_radius = child->radius();
    }

    auto& inst = dst[i];
    inst = {};
    ProceduralTreeRootsRenderer::encode_directions(
      bounds.i, bounds.j, child_bounds.i, child_bounds.j, &inst.directions0, &inst.directions1);
    inst.self_position = node.position;
    inst.self_radius = node.radius();
    inst.child_position = child_pos;
    inst.child_radius = child_radius;
  }
}

std::vector<ProceduralTreeRootsRenderer::Instance>
to_roots_instances(const tree::Internodes& inodes) {
  std::vector<ProceduralTreeRootsRenderer::Instance> instances(inodes.size());
  to_roots_instances(inodes.data(), int(inodes.size()), instances.data());
  return instances;
}

void offset_roots(tree::Internode* inodes, int num_inodes, const Vec3f& off) {
  for (int i = 0; i < num_inodes; i++) {
    inodes[i].translate(off);
  }
}

void rotate_roots(tree::Internode* inodes, int num_inodes, const Vec2f& rot) {
  if (num_inodes == 0) {
    return;
  }

  auto root_off = inodes[0].position;
  inodes[0].position -= root_off;
  auto mat = make_x_rotation(rot.x) * make_y_rotation(rot.y);

  for (int i = 0; i < num_inodes; i++) {
    auto& self = inodes[i];
    self.direction = normalize(to_vec3(mat * Vec4f{self.direction, 0.0f}));
    if (self.has_medial_child()) {
      auto* child = inodes + self.medial_child;
      child->position = self.position + self.length * self.direction;
    }
    if (self.has_lateral_child()) {
      auto* child = inodes + self.lateral_child;
      child->position = self.position;
    }
  }
  for (int i = 0; i < num_inodes; i++) {
    inodes[i].position += root_off;
    inodes[i].render_position = inodes[i].position;
  }
}

void keep_axis(const tree::Internode* inodes, int axis, bool* keep) {
  int ni = axis;
  while (ni != -1) {
    keep[ni] = true;
    ni = inodes[ni].medial_child;
  }
}

int select(const tree::Internode* src, const bool* keep, int num_src,
           int* kept_ind, tree::Internode* dst) {
  std::fill(kept_ind, kept_ind + num_src, -1);

  int ct{};
  for (int i = 0; i < num_src; i++) {
    if (keep[i]) {
      dst[ct] = src[i];
      kept_ind[i] = ct;
      ct++;
    }
  }

  int j{};
  for (int i = 0; i < num_src; i++) {
    if (keep[i]) {
      auto& dst_node = dst[j++];
      if (dst_node.has_parent()) {
        dst_node.parent = kept_ind[dst_node.parent];
      }
      if (dst_node.has_medial_child()) {
        dst_node.medial_child = kept_ind[dst_node.medial_child];
      }
      if (dst_node.has_lateral_child()) {
        dst_node.lateral_child = kept_ind[dst_node.lateral_child];
      }
    }
  }

  return ct;
}

tree::Internodes select(const tree::Internodes& src, const bool* keep) {
  tree::Internodes result(src.size());
  std::vector<int> inds(result.size());
  result.resize(select(src.data(), keep, int(src.size()), inds.data(), result.data()));
  return result;
}

tree::Internodes keep_axis(const tree::Internodes& src, int axis) {
  auto keep = std::make_unique<bool[]>(src.size());
  keep_axis(src.data(), axis, keep.get());
  return select(src, keep.get());
}

Optional<int> ith_axis_root_index(const tree::Internodes& src, int ith) {
  int ct{};
  for (int i = 0; i < int(src.size()); i++) {
    if (src[i].is_axis_root(src)) {
      if (ct++ == ith) {
        return Optional<int>(i);
      }
    }
  }
  return NullOpt{};
}

void require_roots_drawable(ProceduralTreeRootsRenderer::DrawableHandle handle,
                            const std::vector<ProceduralTreeRootsRenderer::Instance>& instances,
                            const UpdateInfo& info) {
  auto& renderer = info.roots_renderer;
  auto& ctx = info.roots_renderer_context;
  renderer.reserve(ctx, handle, uint32_t(instances.size()));
  renderer.fill_activate(ctx, handle, instances.data(), uint32_t(instances.size()));
}

constexpr int max_num_nodes_per_sphere_brush() {
  return 128;
}

struct SphereBrushBoundsData {
public:
  struct AccelModification {
    bool insert;
    int index;
    OBB3f bounds;
  };

  struct Slot {
    bounds::ElementID element_id;
    bool inserted;
  };

public:
  void set_pending_insert(int index, const OBB3f& bounds) {
    assert(!awaiting_accel_modification);
    awaiting_accel_modification = true;
    modification.insert = true;
    modification.index = index;
    modification.bounds = bounds;
  }

  void set_pending_remove(int index) {
    assert(!awaiting_accel_modification);
    awaiting_accel_modification = true;
    modification.insert = false;
    modification.index = index;
  }

  bounds::ElementID remove_at(int index) {
    assert(index >= 0 && index < max_num_nodes_per_sphere_brush());
    assert(slots[index].inserted);
    auto result = slots[index].element_id;
    slots[index].inserted = false;
    slots[index].element_id = {};
    return result;
  }

  void insert_at(int index, bounds::ElementID id) {
    assert(index >= 0 && index < max_num_nodes_per_sphere_brush());
    assert(!slots[index].inserted);
    slots[index].element_id = id;
    slots[index].inserted = true;
  }

public:
  Slot slots[max_num_nodes_per_sphere_brush()];
  bool awaiting_accel_modification;
  AccelModification modification;
};

OBB3f make_sphere_brush_node_bounds(const Vec3f& p0, const Vec3f& p1, float radius) {
  auto axis = p1 - p0;
  auto zlen = axis.length();
  assert(zlen > 0.0f);

  OBB3f result;
  make_coordinate_system_y(axis / zlen, &result.i, &result.k, &result.j); //  @NOTE: swap y, z
  result.position = lerp(0.5f, p0, p1);
  result.half_size.x = radius;
  result.half_size.y = radius;
  result.half_size.z = zlen * 0.5f + radius;
  return result;
}

struct SphereBrush {
  static constexpr int node_capacity = max_num_nodes_per_sphere_brush();

  enum class State {
    Idle = 0,
    Forwards,
    AwaitingReverse,
    Reverse
  };

  State state;
  Vec3f p0;
  Vec3f p1;
  Vec3f current_position;
  Vec3f initial_position;
  Vec3f axis;
  float length;
  float t;
  float current_radius;
  bool can_recede;

  Vec3f position_history[node_capacity];
  float radius_history[node_capacity];
  int node_index;
  int max_num_nodes;
};

struct GlobalData {
  Vec3f sphere_p;
  float sphere_r;
  transform::TransformInstance* sphere_tform;
  bool did_init{};

  SphereBrush debug_wall_brush{};
  SphereBrushBoundsData debug_brush_bounds_data{};

  VoxelSamples voxel_samples;
  CubeMarchMeshData mesh_data;

  PlaceOnMeshResult latest_place_on_mesh_result;
  TerrainRenderer::TerrainGrassDrawableHandle grass_drawable{};
  ProceduralTreeRootsRenderer::DrawableHandle debug_roots_drawable{};
  tree::Internodes debug_roots_internodes;
  tree::Internodes transformed_roots_internodes;
  Vec3f roots_drawable_offset{8.0f, 16.0f, 8.0f};
  Vec2f roots_rot{};
  Vec2f last_roots_rot{};
  transform::TransformInstance* roots_tform{};

  transform::TransformInstance* hole_tforms[32]{};
  int num_holes{4};

} global_data;

struct UpdateCubeMarchResult {
  transform::TransformInstance* tform_insts[32];
  int num_add;
};

SphereBrush make_sphere_brush(const Vec3f& start_position, float length, int max_num_nodes) {
  assert(max_num_nodes <= SphereBrush::node_capacity);
  SphereBrush brush{};
  const auto axis = Vec3f{0.0f, 0.0f, -1.0f};
  brush.length = length;
  brush.p0 = start_position;
  brush.p1 = start_position + axis * length;
  brush.initial_position = start_position;
  brush.current_position = start_position;
  brush.axis = axis;
  brush.state = SphereBrush::State::Forwards;
  brush.current_radius = 8.0f;
  for (int i = 0; i < max_num_nodes; i++) {
    brush.position_history[i] = start_position;
    brush.radius_history[i] = brush.current_radius;
  }
  brush.max_num_nodes = max_num_nodes;
  return brush;
}

template <typename F>
auto update_sphere_brush(SphereBrush& brush, float dt, float speed, const F& next_axis) {
  struct Result {
    Vec3f curr_p;
    Vec3f prev_p;
    float radius;
    OBB3f new_bounds;
    int bounds_index;
    bool insert_bounds;
    bool remove_bounds;
  };

  Result result{};

  switch (brush.state) {
    case SphereBrush::State::Forwards:
    case SphereBrush::State::Reverse: {
#if 0
      if (brush.state == SphereBrush::State::Forwards) {
        if (height_adjust > 0) {
          brush.p1.y += 4.0f;
        } else if (height_adjust < 0) {
          brush.p1.y -= 4.0f;
        }
      }
#endif

      brush.t = std::min(brush.t + dt * speed, 1.0f);

      const auto prev_p = brush.current_position;
      const auto curr_p = lerp(brush.t, brush.p0, brush.p1);
      brush.current_position = curr_p;
      brush.position_history[brush.node_index] = brush.current_position;
      brush.radius_history[brush.node_index] = brush.current_radius;

      result.prev_p = prev_p;
      result.curr_p = curr_p;
      result.radius = brush.current_radius;

      if (brush.t == 1.0f) {
        if (brush.state == SphereBrush::State::Forwards) {
          if (brush.node_index + 1 == brush.max_num_nodes) {
            brush.state = SphereBrush::State::AwaitingReverse;
          } else {
            brush.axis = next_axis(brush);
            ++brush.node_index;
            brush.position_history[brush.node_index] = brush.current_position;
            brush.radius_history[brush.node_index] = brush.current_radius;
            brush.t = 0.0f;
            brush.p0 = brush.current_position;
            brush.p1 = brush.p0 + brush.axis * brush.length;
            //  Add new bounds
            result.new_bounds = make_sphere_brush_node_bounds(
              brush.p0, brush.p1, brush.current_radius);
            result.insert_bounds = true;
            result.bounds_index = brush.node_index;
          }
        } else {
          //  Remove old bounds
          result.remove_bounds = true;
          result.bounds_index = brush.node_index;

          if (brush.node_index == 0) {
            brush.state = SphereBrush::State::Idle;
          } else {
            brush.position_history[brush.node_index--] = brush.initial_position;
            brush.t = 0.0f;
            brush.p0 = brush.current_position;
            brush.p1 = brush.node_index == 0 ?
              brush.initial_position : brush.position_history[brush.node_index - 1];
          }
        }
      }
      break;
    }
    case SphereBrush::State::AwaitingReverse: {
      if (brush.can_recede) {
        brush.state = SphereBrush::State::Reverse;
        brush.t = 0.0f;
        brush.p0 = brush.position_history[brush.max_num_nodes - 1];
        brush.p1 = brush.max_num_nodes == 1 ?
          brush.initial_position : brush.position_history[brush.max_num_nodes - 2];
        brush.node_index = brush.max_num_nodes - 1;
      }
      break;
    }
    default: {
      break;
    }
  }

  return result;
}

void on_need_modify_sphere_brush_bounds(SphereBrushBoundsData& brush_bounds,
                                        bool insert_bounds, bool remove_bounds,
                                        int bounds_index, const OBB3f& new_bounds) {
  assert(insert_bounds != remove_bounds || (!insert_bounds && !remove_bounds));
  if (insert_bounds) {
    brush_bounds.set_pending_insert(bounds_index, new_bounds);
  } else if (remove_bounds) {
    brush_bounds.set_pending_remove(bounds_index);
  }
}

void update_sphere_brush_bounds_pending_modification(DebugTerrainComponent& component,
                                                     SphereBrushBoundsData& brush_bounds,
                                                     bounds::BoundsSystem* bounds_system,
                                                     bounds::AccelInstanceHandle accel_handle) {
  assert(brush_bounds.awaiting_accel_modification);

  const auto accel_accessor = component.bounds_accessor;
  auto* accel = bounds::request_write(bounds_system, accel_handle, accel_accessor);
  if (!accel) {
    return;
  }

  auto& mod = brush_bounds.modification;
  if (mod.insert) {
    assert(mod.index > 0);
    const auto terrain_tag = component.bounds_element_tag;
    auto el_id = bounds::ElementID::create();
    auto el = bounds::make_element(mod.bounds, el_id.id, 0, terrain_tag.id);
    accel->insert(std::move(el));
    brush_bounds.insert_at(mod.index, el_id);
  } else if (mod.index > 0) {
    auto rem_id = brush_bounds.remove_at(mod.index);
    size_t num_deactivated = bounds::deactivate_element(accel, rem_id);
    assert(num_deactivated == 1);
    (void) num_deactivated;
  }

  bounds::release_write(bounds_system, accel_handle, accel_accessor);
  //  Finished modifying.
  brush_bounds.awaiting_accel_modification = false;
}

void cube_march_sphere_brush(DebugTerrainComponent& component, ChunkIndices& chunks,
                             const UpdateInfo& info) {
  auto& cube_march_params = component.cube_march_params;

  int height_adjust{};
  if (cube_march_params.need_increase_wall_height) {
    height_adjust = 1;
    cube_march_params.need_increase_wall_height = false;
  } else if (cube_march_params.need_decrease_wall_height) {
    height_adjust = -1;
    cube_march_params.need_decrease_wall_height = false;
  }

  auto& brush = global_data.debug_wall_brush;
  if (cube_march_params.need_initialize_wall) {
    brush = make_sphere_brush(global_data.sphere_p, 2.0f, 128);
    cube_march_params.need_initialize_wall = false;
  }

#if 1
  auto next_axis = [&](const SphereBrush& brush) -> Vec3f {
    float circ_scale = cube_march_params.wall_brush_circle_scale;
    if (cube_march_params.brush_control_by_instrument) {
      circ_scale = cube_march_params.instrument_brush_circle_scale;
    }

    float axis_y = lerp(urandf(), -0.05f, 0.05f);
    if (cube_march_params.height_index == 0 && brush.node_index % 16 == 0) {
      if (cube_march_params.cumulative_height_index > 0) {
        cube_march_params.need_decrease_wall_height = true;
        cube_march_params.height_index = -1;
        cube_march_params.cumulative_height_index = -1;
        axis_y = -lerp(urandf(), 0.25f, 0.5f);
      } else {
        cube_march_params.need_increase_wall_height = true;
        cube_march_params.height_index = 1;
        cube_march_params.cumulative_height_index = 1;
        axis_y = lerp(urandf(), 0.25f, 0.5f);
      }
    } else if (cube_march_params.height_index != 0 && brush.node_index % 8 == 0) {
      if (cube_march_params.height_index < 0) {
        cube_march_params.need_increase_wall_height = true;
        cube_march_params.height_index = 0;
      } else {
        cube_march_params.need_decrease_wall_height = true;
        cube_march_params.height_index = 0;
      }
    }

    auto rot = make_rotation(pif() * circ_scale) * normalize(Vec2f{brush.axis.x, brush.axis.z});
    auto p1 = brush.p0 + Vec3f{rot.x, 0.0f, rot.y} * brush.length;
    auto h = info.terrain.height_nearest_position_xz(p1) - 4.0f;
    axis_y += (h - brush.p0.y) * 0.1f;

//      float axis_y = float(cube_march_params.height_index) * 0.25f;
//    auto axis = normalize(Vec3f{rot.x, brush.axis.y + axis_y, rot.y});
    auto axis = normalize(Vec3f{rot.x, axis_y, rot.y});

    return axis;
  };
#else
  auto next_axis = [&](const SphereBrush& brush) {
      auto rand_axis_weight = cube_march_params.wall_random_axis_weight;
      return normalize(
        brush.axis + rand_axis_weight * Vec3f{urand_11f(), 0.0f, urand_11f()});
    };
#endif
  //
  constexpr int max_num_holes = 64;
  assert(global_data.num_holes <= max_num_holes);
  OBB3f hole_obbs[max_num_holes];
  int num_holes{};

#if 1
  for (int i = 0; i < std::min(info.num_tree_aabbs, max_num_holes); i++) {
    auto& bounds = info.tree_aabbs[i];
    auto& base_p = info.tree_base_positions[i];
    auto size = bounds.size() * 0.5f;
    size *= Vec3f{0.25f, 2.0f, 0.25f};
//      size *= Vec3f{1.0f, 2.0f, 1.0f};
    size = max(size, Vec3f{4.0f, -1.0f, 4.0f});

    auto center = bounds.center();
    center = Vec3f{base_p.x, center.y, base_p.z};
    hole_obbs[num_holes] = OBB3f::axis_aligned(center, size);

    if (cube_march_params.draw_bounds) {
      vk::debug::draw_obb3(hole_obbs[num_holes], Vec3f{1.0f, 0.0f, 0.0f});
    }

    num_holes++;
  }

  int rem_holes = max_num_holes - num_holes;
  for (int i = 0; i < std::min(info.num_wall_bounds, rem_holes); i++) {
    auto bounds = info.wall_bounds[i];
    bounds.half_size.y = std::max(32.0f, bounds.half_size.y);
    bounds.half_size.x = std::max(6.0f, bounds.half_size.x);
    bounds.half_size.z = std::max(6.0f, bounds.half_size.z);
    hole_obbs[num_holes++] = bounds;
  }
#else
  for (int i = 0; i < global_data.num_holes; i++) {
      if (auto* tform = global_data.hole_tforms[i]) {
        auto curr = tform->get_current();
        hole_ps[num_holes] = curr.translation;
        hole_rs[num_holes] = 8.0f;
        hole_obbs[num_holes] = OBB3f::axis_aligned(curr.translation, curr.scale);
        num_holes++;
      }
    }
#endif
  //

  float brush_speed = cube_march_params.wall_brush_speed;
  if (cube_march_params.brush_control_by_instrument) {
    if (cube_march_params.instrument_brush_speed) {
      brush_speed = cube_march_params.instrument_brush_speed.value();
    } else {
      brush_speed = 0.0f;
    }
  }

  auto& brush_bounds = global_data.debug_brush_bounds_data;
  if (brush_bounds.awaiting_accel_modification) {
    update_sphere_brush_bounds_pending_modification(
      component, brush_bounds, info.bounds_system, info.accel_handle);

  } else {
    auto res = update_sphere_brush(brush, float(info.real_dt), brush_speed, next_axis);
    auto [p0, sz] = moved_sphere_to_quantized_span(
      res.prev_p, res.radius, res.curr_p, res.radius, define_grid());
    insert_chunk_indices(chunks, p0, sz, CubeMarchMeshData::chunk_dim);
    move_sphere(
      p0, sz, define_grid(), global_data.voxel_samples,
      brush.position_history, brush.radius_history, brush.max_num_nodes,
      hole_obbs, num_holes);

    on_need_modify_sphere_brush_bounds(
      brush_bounds, res.insert_bounds, res.remove_bounds, res.bounds_index, res.new_bounds);
  }

//    cube_march_params.need_recompute = true;
  cube_march_params.instrument_brush_speed = NullOpt{};
}

void voronoi_1d(float* segments, int num_segments, float* ps, int num_ps) {
  for (int i = 0; i < num_ps; i++) {
    ps[i] = urandf();
  }

  for (int i = 0; i < num_segments; i++) {
    float f = float(i) / float(num_segments);
    int mi{};
    float mn{infinityf()};
    for (int j = 0; j < num_ps; j++) {
      float d = std::abs(ps[j] - f);
      if (d < mn) {
        mn = d;
        mi = j;
      }
    }
    segments[i] = float(mi) / float(num_ps - 1);
  }
}

std::vector<OBB3f> add_rock(
  ChunkIndices& chunks, const cm::GridInfo& grid, const UpdateInfo& info,
  const Vec3f& start_p, float orient_theta, float sphere_r) {
  //
  float theta{};
  auto rot_z = make_z_rotation(orient_theta);

  Bounds3f bounds;
  while (theta < pif()) {
    Vec2f v{std::cos(theta), std::sin(theta)};
    Vec3f base_p{v.x, v.y, 0.0f};
    auto p = to_vec3(rot_z * Vec4f{base_p, 0.0f}) + start_p;

    const float terrain_height = info.terrain.height_nearest_position_xz(p);
    p.y += terrain_height;

    adjust_in_radius(p, sphere_r, grid, global_data.voxel_samples, false, chunks);
    bounds = union_of(bounds, Bounds3f{p - sphere_r, p + sphere_r});

    theta += 0.1f;
  }

  std::vector<OBB3f> result;
  result.push_back(OBB3f::axis_aligned(bounds.center(), bounds.size() * 0.5f));
  return result;
}

std::vector<OBB3f> add_arch(
  ChunkIndices& chunks, const cm::GridInfo& grid, const UpdateInfo& info,
  const Vec3f& start_p, float orient_theta, float arch_r, float sphere_r) {
  //
  float theta{};
  auto rot = make_y_rotation(orient_theta);

  const float terrain_height = info.terrain.height_nearest_position_xz(start_p);

  std::vector<OBB3f> tmp;
  std::vector<OBB3f> result;
  while (theta < pif()) {
    float theta_noise = urand_11f() * 0.05f;

    float r_adjust_frac = std::pow(clamp01(theta / pif()), 4.0f);
    const float eval_r = arch_r + r_adjust_frac * 4.0f;

    Vec2f v{std::cos(theta + theta_noise), std::sin(theta + theta_noise)};
    Vec3f base_p{v.x * eval_r, v.y * eval_r, 0.0f};
    auto p = to_vec3(rot * Vec4f{base_p, 0.0f}) + start_p;
    p.y += terrain_height;
    p += Vec3f{urand_11f(), 0.0f, urand_11f()} * 1.0f;

    adjust_in_radius(p, sphere_r, grid, global_data.voxel_samples, false, chunks);
    theta += 0.1f;

    tmp.push_back(OBB3f::axis_aligned(p, Vec3f{sphere_r}));
  }

  result.resize(tmp.size());
  bounds::FitOBBsAroundAxisParams fit{};
  fit.axis_bounds = tmp.data();
  fit.num_bounds = int(tmp.size());
  fit.max_size_ratio = Vec3f{1.5f};
  fit.test_type = bounds::FitOBBsAroundAxisParams::TestType::SizeRatio;
  fit.dst_bounds = result.data();
  result.resize(bounds::fit_obbs_around_axis(fit));
  return result;
}

[[maybe_unused]]
std::vector<OBB3f> add_wall(
  ChunkIndices& chunks, const cm::GridInfo& grid, const UpdateInfo& info,
  const Vec3f& start_p, float orient_theta, float wall_len) {
  //
  const float step_size = 2.0f;
  const float sphere_r = 4.0f;

  Vec2f step{};
  float step_len{};
  Vec2f rot_xz{std::cos(orient_theta), std::sin(orient_theta)};

  const Vec3f iv{rot_xz.x, 0.0f, rot_xz.y};
  const Vec3f jv{0.0f, 1.0f, 0.0f};
  const Vec3f kv{-rot_xz.y, 0.0f, rot_xz.x};
  OBB3f base_bounds{iv, jv, kv, {}, Vec3f{sphere_r}};

  Bounds3f dst_bounds{};
  std::vector<OBB3f> result;

  Vec3f p_sum{};
  float num_ps{};
  while (step.length() < wall_len) {
    Vec3f accum_p{step_len, 0.0f, 0.0f};
    Vec3f off_p = Vec3f{urand_11f(), urand_11f(), urand_11f()} * Vec3f{2.0f, 1.0f, 2.0f};
    accum_p += off_p;

    Vec3f p{start_p.x + step.x, 0.0f, start_p.z + step.y};
    p += off_p;

    const float terrain_height = info.terrain.height_nearest_position_xz(p);
    p.y += terrain_height;
    accum_p.y += terrain_height;

    adjust_in_radius(p, sphere_r, grid, global_data.voxel_samples, false, chunks);

    step += rot_xz * step_size;
    step_len += step_size;

    auto curr_bounds = Bounds3f{accum_p - sphere_r, accum_p + sphere_r};
    dst_bounds = union_of(dst_bounds, curr_bounds);

    result.push_back(OBB3f::axis_aligned(p, Vec3f{sphere_r}));
    p_sum += p;
    num_ps += 1.0f;
  }

  base_bounds.half_size = dst_bounds.size() * 0.5f;
  base_bounds.position = p_sum / num_ps;
  result.clear();
  result.push_back(base_bounds);
//  base_bounds.position = mid_p;
//  return base_bounds;
  return result;
}

std::vector<OBB3f> add_rocks(
  ChunkIndices& chunks, const cm::GridInfo& grid, const UpdateInfo& info,
  const Vec3f& start_p, float rock_r, float rock_scale, float rock_rand_scale) {
  //
  constexpr int stack_size = 128;
  Temporary<Vec2f, stack_size> store_dst_ps;
  Temporary<bool, stack_size> store_accept_ps;

  const int num_rocks = 8;
  auto* dst_ps = store_dst_ps.require(num_rocks);
  auto* accept_ps = store_accept_ps.require(num_rocks);
  float place_r = points::place_outside_radius_default_radius(num_rocks, 0.9f);
  points::place_outside_radius<Vec2f, float, 2>(dst_ps, accept_ps, num_rocks, place_r);

  std::vector<OBB3f> result;
  for (int i = 0; i < num_rocks; i++) {
    auto p2 = dst_ps[i] * 2.0f - 1.0f;
    auto wall_p = Vec3f{p2.x, 0.0f, p2.y} * rock_r + start_p;
    const float theta = urand_11f() * pif();
    auto bounds = add_rock(
      chunks, grid, info, wall_p, theta, rock_scale + urand_11f() * rock_rand_scale);
    result.insert(result.end(), bounds.begin(), bounds.end());
  }

  return result;
}

std::vector<OBB3f> add_components(
  ChunkIndices& chunks, const cm::GridInfo& grid, const UpdateInfo& info) {
  //
  std::vector<OBB3f> result;

  const float r = 128.0f;
  const float r2 = 72.0f;

  {
    for (auto& b : add_arch(chunks, grid, info, Vec3f{r, 0.0f, r}, 0.0f, 24.0f, 12.0f)) {
      result.push_back(b);
    }
    for (auto& b : add_arch(chunks, grid, info, Vec3f{r, 0.0f, r}, pif() * 0.25f, 24.0f, 12.0f)) {
      result.push_back(b);
    }
    for (auto& b : add_arch(chunks, grid, info, Vec3f{-r, 0.0f, -r}, 0.0f, 12.0f, 8.0f)) {
      result.push_back(b);
    }
    for (auto& b : add_arch(chunks, grid, info, Vec3f{-r, 0.0f, -r}, pif() * 0.5f, 12.0f, 8.0f)) {
      result.push_back(b);
    }
  }

  {
    const int num_rock_patches = 5;
    const Vec2f rock_p2s[num_rock_patches]{
      Vec2f{r, r}, Vec2f{-r2, r2}, Vec2f{-r2, -r2}, Vec2f{r2, -r2},
      Vec2f{42.0f, 60.0f}
    };

    for (int i = 0; i < num_rock_patches; i++) {
      auto p0 = Vec3f{rock_p2s[i].x, 0.0f, rock_p2s[i].y};
      auto rock_bounds = add_rocks(chunks, grid, info, p0, 16.0f, 4.0f, 1.0f);
      result.insert(result.end(), rock_bounds.begin(), rock_bounds.end());
    }
  }

  return result;
}

[[maybe_unused]]
void gen_cube_march_circle_wall(DebugTerrainComponent&, ChunkIndices& chunks,
                                const cm::GridInfo& grid, const UpdateInfo& info) {
#if 1
  const int num_segments = 128;
  float voronoi_segments0[num_segments];
  float voronoi_segments1[num_segments];

  {
    const int num_ps = 16;
    float voronoi_ps[num_ps];
    voronoi_1d(voronoi_segments0, num_segments, voronoi_ps, num_ps);
    voronoi_1d(voronoi_segments1, num_segments, voronoi_ps, num_ps);

    for (int i = 0; i < num_segments; i++) {
      voronoi_segments0[i] = std::pow(voronoi_segments0[i], 4.0f);
    }
  }
#endif

  const float r = Terrain::terrain_dim * 0.5f - 16.0f;
  const float sphere_r = 8.0f;
  const float step_size = 4.0f;
  float theta{};

  while (theta < float(two_pi())) {
    auto p_xz = Vec2f{std::cos(theta), std::sin(theta)} * (r + urand_11f() * 4.0f);

    Vec3f p{p_xz.x, 0.0f, p_xz.y};
    const float terrain_height = info.terrain.height_nearest_position_xz(p);
    p.y = terrain_height - sphere_r * 0.5f;

    float s = clamp01(theta / float(two_pi()));
    float height_seg = voronoi_segments0[clamp(int(s * float(num_segments)), 0, num_segments-1)];
    float step_seg = voronoi_segments1[clamp(int(s * float(num_segments)), 0, num_segments-1)];
    p.y -= height_seg * 2.0f;

    adjust_in_radius(p, sphere_r, grid, global_data.voxel_samples, false, chunks);

    float ss = step_size + (step_seg * 2.0f - 1.0f) * step_size * 0.25f;
    theta += std::abs(std::asin(ss / r));
  }
}

[[maybe_unused]]
void gen_cube_march_square_wall(DebugTerrainComponent&, ChunkIndices& chunks,
                                const cm::GridInfo& grid, const UpdateInfo& info) {

  const int num_segments = 128;
  float voronoi_segments0[num_segments];
  float voronoi_segments1[num_segments];

  {
    const int num_ps = 16;
    float voronoi_ps[num_ps];
    voronoi_1d(voronoi_segments0, num_segments, voronoi_ps, num_ps);
    voronoi_1d(voronoi_segments1, num_segments, voronoi_ps, num_ps);

    for (int i = 0; i < num_segments; i++) {
      voronoi_segments0[i] = std::pow(voronoi_segments0[i], 4.0f);
    }
  }

  float offset = 32.0f;
  float sphere_r = 8.0f;
  float step_size = 4.0f;

  for (int i = 0; i < 4; i++) {
    const float edge0 = -Terrain::terrain_dim * 0.5f + offset;
    float edge1 = ((i % 2) == 0) ? Terrain::terrain_dim * 0.5f - offset : edge0;

    float v{edge0};
    while (true) {
      float v0 = v;
      float v1 = v0 + sphere_r;
      if (v1 > Terrain::terrain_dim * 0.5f - offset) {
        break;
      }

      float c = v0 + sphere_r * 0.5f;
      Vec3f p = i < 2 ? Vec3f{c, 0.0f, edge1} : Vec3f{edge1, 0.0f, c};
      const Vec3f n = i < 2 ? Vec3f{0.0f, 0.0f, 1.0f} : Vec3f{1.0f, 0.0f, 0.0f};

      const float terrain_height = info.terrain.height_nearest_position_xz(p);
      p.y = terrain_height - sphere_r * 0.5f;
      p.y += urand_11f() * 0.125f;
      p += n * urand_11f() * 8.0f;

      float s = clamp(((v - offset) * 2.0f) / Terrain::terrain_dim, -1.0f, 1.0f) * 0.5f + 0.5f;
      float height_seg = voronoi_segments0[clamp(int(s * float(num_segments)), 0, num_segments-1)];
      float step_seg = voronoi_segments1[clamp(int(s * float(num_segments)), 0, num_segments-1)];
      p.y -= height_seg * 2.0f;

      adjust_in_radius(p, sphere_r, grid, global_data.voxel_samples, false, chunks);
      v += step_size + (step_seg * 2.0f - 1.0f) * step_size * 0.25f;
    }
  }
};

void maybe_insert_component_bounds(DebugTerrainComponent& component, const UpdateInfo& info) {
  if (!component.need_insert_component_bounds) {
    return;
  }

  const auto accel_accessor = component.bounds_accessor;
  auto* accel = bounds::request_write(info.bounds_system, info.accel_handle, accel_accessor);
  if (!accel) {
    return;
  }

  //  radius limiter
  for (auto& bounds : component.component_bounds) {
    auto el = bounds::RadiusLimiterElement::create_enclosing_obb3(
      bounds, component.radius_limiter_aggregate_id, component.radius_limiter_element_tag);
    (void) bounds::insert(info.radius_limiter, el, false);
  }

  //  other accel
  for (auto& bounds : component.component_bounds) {
    const auto id = bounds::ElementID::create();
    accel->insert(bounds::make_element(bounds, id.id, id.id, component.bounds_element_tag.id));
  }

  bounds::release_write(info.bounds_system, info.accel_handle, accel_accessor);
  component.need_insert_component_bounds = false;
}

UpdateCubeMarchResult update_cube_march(DebugTerrainComponent& component, ChunkIndices& chunks,
                                        const UpdateInfo& info) {
  UpdateCubeMarchResult result{};

  auto grid = define_grid();
  if (!global_data.did_init) {
    global_data.sphere_p = Vec3f{32.0f, 8.0f, 32.0f};
    global_data.sphere_r = 8.0f;

    global_data.sphere_tform = info.tform_system.create(
      TRS<float>::make_translation_scale(global_data.sphere_p, Vec3f{global_data.sphere_r}));
    result.tform_insts[result.num_add++] = global_data.sphere_tform;
    global_data.did_init = true;
  }

  adjust_in_radius(
    global_data.sphere_p,
    global_data.sphere_r,
    grid,
    global_data.voxel_samples, component.cube_march_params.invert, chunks);

  if (!global_data.transformed_roots_internodes.empty()) {
    auto root_obb = tree::internode_obb(global_data.transformed_roots_internodes[0]);
    root_obb.half_size = Vec3f{4.0f, 32.0f, 4.0f};
    insert_obb_hole(root_obb, grid, global_data.voxel_samples, chunks);

    if (component.cube_march_params.draw_bounds) {
      vk::debug::draw_obb3(root_obb, Vec3f{0.0f, 1.0f, 1.0f});
    }
  }

  for (int i = 0; i < global_data.num_holes; i++) {
    if (auto* tform = global_data.hole_tforms[i]) {
      auto trs = tform->get_current();
      auto obb = OBB3f::axis_aligned(trs.translation, trs.scale);
      insert_obb_hole(obb, grid, global_data.voxel_samples, chunks);
    }
  }

  return result;
}

auto debug_place_on_mesh(const DebugTerrainComponent& component,
                         const CubeMarchMeshData& mesh_data) {
  std::vector<Vec3f> ps;
  std::vector<Vec3f> ns;
  for (auto& [_, chunk] : mesh_data.chunks.cache) {
    for (auto& v : chunk.vertices) {
      ps.push_back(v.position);
      ns.push_back(v.normal);
    }
  }
  return debug_place_on_mesh(ps, ns, component.place_on_mesh_params.obb3_size);
}

void debug_place_grass_on_mesh(const DebugTerrainComponent& component,
                               const PlaceOnMeshResult& result,
                               TerrainRenderer::TerrainGrassDrawableHandle* draw_handle,
                               const UpdateInfo& info) {
  std::vector<TerrainRenderer::TerrainGrassInstance> instances;
  for (int i = 0; i < int(result.point_entries.size()); i++) {
    auto& entry = result.point_entries[i];
    auto up = result.bounds[entry.obb3_index].j;
    if (up.y > component.place_on_mesh_params.normal_y_threshold) {
      auto& inst = instances.emplace_back();
      inst.translation_rand01 = Vec4f{entry.position, urandf()};
      inst.direction_unused = Vec4f{up, 0.0f};
    }
  }

  const auto num_instances = uint32_t(instances.size());
  auto& ctx = info.terrain_renderer_context;
  info.terrain_renderer.reserve(ctx, draw_handle, num_instances);
  info.terrain_renderer.set_instances(ctx, *draw_handle, instances.data(), num_instances);
}

void update_roots(DebugTerrainComponent& component, const UpdateInfo& info,
                  DebugTerrainComponent::UpdateResult& result) {
  auto& node_params = component.nodes_through_terrain_params;
  if (global_data.roots_tform) {
    auto curr = global_data.roots_tform->get_current();
    if (curr.translation != global_data.roots_drawable_offset) {
      global_data.roots_drawable_offset = curr.translation;
      node_params.need_update_roots_drawable = true;
    }
  }

  if (global_data.roots_rot.x != global_data.last_roots_rot.x ||
      global_data.roots_rot.y != global_data.last_roots_rot.y) {
    node_params.need_update_roots_drawable = true;
  }
  global_data.last_roots_rot = global_data.roots_rot;

  if (!global_data.debug_roots_drawable.is_valid()) {
    if (auto roots = read_root_internodes()) {
      global_data.debug_roots_internodes = std::move(roots.value());
      global_data.debug_roots_drawable = info.roots_renderer.create(
        ProceduralTreeRootsRenderer::DrawableType::NoWind);
      node_params.need_update_roots_drawable = true;
    }
  }

  if (node_params.need_update_roots_drawable && global_data.debug_roots_drawable.is_valid()) {
    auto roots = global_data.debug_roots_internodes;
    if (node_params.keep_axis) {
      if (auto ind = ith_axis_root_index(roots, node_params.keep_ith_axis)) {
        roots = keep_axis(roots, ind.value());
      }
    }

    offset_roots(roots.data(), int(roots.size()), global_data.roots_drawable_offset);
    rotate_roots(roots.data(), int(roots.size()), global_data.roots_rot);

    auto insts = to_roots_instances(roots);
    global_data.transformed_roots_internodes = std::move(roots);
    require_roots_drawable(global_data.debug_roots_drawable, insts, info);
#if 1
    info.roots_renderer.set_hidden(global_data.debug_roots_drawable, true);
#endif
    node_params.need_update_roots_drawable = false;
  }

  if (!global_data.roots_tform) {
    auto trs = TRS<float>::make_translation(global_data.roots_drawable_offset);
    global_data.roots_tform = info.tform_system.create(trs);
    AddTransformEditor add{};
    add.inst = global_data.roots_tform;
    add.color = Vec3f{0.0f, 0.0f, 1.0f};
    result.add_tform_editors[result.num_add++] = add;
  }
}

} //  anon

DebugTerrainComponent::UpdateResult DebugTerrainComponent::update(const UpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("DebugTerrainComponent/update");
  (void) profiler;

  UpdateResult result{};

  info.terrain_renderer.set_cube_march_geometries_hidden(cube_march_params.hidden);

  if (debug_models.empty()) {
    debug_models.emplace_back();
  }

  if (debug_model_index < int(debug_models.size())) {
    update_debug_geometry(*this, debug_models[debug_model_index], info);
    update_debug_image(*this, debug_models[debug_model_index], info);
  }

  update_roots(*this, info, result);

  if (!ground_color_image.is_valid() && !tried_load_color_image) {
    color_image_file_path = std::string{GROVE_ASSET_DIR} + "/textures/terrain/green.png";
    tried_load_color_image = true;
  }
  if (!splotch_image.is_valid() && !tried_load_splotch_image) {
    splotch_image_file_path = std::string{GROVE_ASSET_DIR} + "/textures/terrain/splotch.png";
    tried_load_splotch_image = true;
  }

  if (auto splotch_im = update_splotch_image(*this, info)) {
    result.new_splotch_image = splotch_im.value();
  }
  if (auto color_im = update_ground_color_image(*this, info)) {
    result.new_ground_color_image = color_im.value();
  }

  if (debug_tforms.size() < debug_models.size()) {
    debug_tforms.push_back(info.tform_system.create(TRS<float>::identity()));
    AddTransformEditor add{};
    add.inst = debug_tforms.back();
    add.color = Vec3f{0.0f, 0.0f, 1.0f};
    result.add_tform_editors[result.num_add++] = add;

  } else if (debug_model_index < int(debug_tforms.size()) &&
             debug_model_index < int(debug_models.size())) {
    update_debug_drawable(debug_models[debug_model_index], debug_tforms[debug_model_index], info);
  }

  if (global_data.sphere_tform) {
    auto trs = global_data.sphere_tform->get_current();
    if (trs.translation != global_data.sphere_p) {
      global_data.sphere_p = trs.translation;
      if (cube_march_params.active) {
        cube_march_params.need_recompute = true;
      }
    }
  }

  ChunkIndices chunks;

  if (cube_march_params.use_wall_brush) {
    cube_march_sphere_brush(*this, chunks, info);
  }

  if (cube_march_params.need_recompute) {
    auto cube_update_res = update_cube_march(*this, chunks, info);
    for (int i = 0; i < cube_update_res.num_add; i++) {
      AddTransformEditor add{};
      add.inst = cube_update_res.tform_insts[i];
      add.color = Vec3f{1.0f};
      result.add_tform_editors[result.num_add++] = add;
    }
    cube_march_params.need_recompute = false;
  }

  if (!cube_march_params.made_perimeter_wall) {
    gen_cube_march_circle_wall(*this, chunks, define_grid(), info);
#if 1
    //  disabled, with normals: 41712
    //  disabled, no normals: 41550
    //  enabled, no normals: 46572
    //  enabled, with normals: 46128
    component_bounds = add_components(chunks, define_grid(), info);
    need_insert_component_bounds = true;
#endif
    cube_march_params.made_perimeter_wall = true;
  }

  regen_chunks(define_grid(), global_data.voxel_samples, chunks, global_data.mesh_data, info);

  maybe_insert_component_bounds(*this, info);

  if (cube_march_params.draw_bounds) {
    for (auto& bounds : component_bounds) {
      vk::debug::draw_obb3(bounds, Vec3f{1.0f, 0.0f, 1.0f});
    }
  }

  if (cube_march_params.need_clear) {
    global_data.voxel_samples.clear();
    global_data.mesh_data.clear(info.terrain_renderer);
    cube_march_params.need_clear = false;
  }

  if (place_on_mesh_params.need_recompute && global_data.did_init) {
    global_data.latest_place_on_mesh_result = debug_place_on_mesh(*this, global_data.mesh_data);
    auto& res = global_data.latest_place_on_mesh_result;
    debug_place_grass_on_mesh(*this, res, &global_data.grass_drawable, info);
    place_on_mesh_params.need_recompute = false;
  }

  if (place_on_mesh_params.draw_result) {
    for (auto& entry : global_data.latest_place_on_mesh_result.point_entries) {
      vk::debug::draw_cube(entry.position, Vec3f{0.1f}, Vec3f{1.0f, 0.0f, 0.0f});
    }
    for (auto& obb : global_data.latest_place_on_mesh_result.bounds) {
      vk::debug::draw_obb3(obb, Vec3f{0.0f, 0.0f, 1.0f});
    }
  }

  for (int i = 0; i < global_data.num_holes; i++) {
    if (!global_data.hole_tforms[i]) {
      auto scl = Vec3f{8.0f, 32.0f, 8.0f};
      auto trans = TRS<float>::make_translation_scale(Vec3f{16.0f, 8.0f, 8.0f}, scl);
      global_data.hole_tforms[i] = info.tform_system.create(trans);

      AddTransformEditor add{};
      add.inst = global_data.hole_tforms[i];
      add.color = Vec3f{0.0f, 1.0f, 0.0f};
      result.add_tform_editors[result.num_add++] = add;
    } else if (cube_march_params.draw_bounds) {
      auto trs = global_data.hole_tforms[i]->get_current();
      auto obb = OBB3f::axis_aligned(trs.translation, trs.scale);
      vk::debug::draw_obb3(obb, Vec3f{0.0f, 1.0f, 1.0f});
    }
  }

  if (global_data.did_init) {
    const Vec3f sphere_p = global_data.sphere_p;
    const float sphere_r = global_data.sphere_r;
    Bounds3f box{sphere_p - sphere_r, sphere_p + sphere_r};
    vk::debug::draw_aabb3(box, Vec3f{0.0f, 1.0f, 1.0f});
  }

  return result;
}

DebugTerrainComponent::CubeMarchStats DebugTerrainComponent::get_cube_march_stats() const {
  CubeMarchStats result{};
  result.num_cube_march_vertices = global_data.mesh_data.total_num_vertices();
  result.num_cube_march_triangles = global_data.mesh_data.total_num_triangles();
  result.num_cube_march_chunks = global_data.mesh_data.num_chunks();
  result.num_voxel_blocks = global_data.voxel_samples.num_blocks();
  result.num_voxel_samples = global_data.voxel_samples.num_samples();
  return result;
}

float DebugTerrainComponent::get_cube_march_editor_radius() const {
  return global_data.sphere_r;
}

Optional<Vec2f> DebugTerrainComponent::get_roots_rotation() const {
  return Optional<Vec2f>(global_data.roots_rot);
}

void DebugTerrainComponent::set_brush_speed01(float v) {
  cube_march_params.instrument_brush_speed = lerp(clamp01(v), 0.0f, 2.0f);
}

void DebugTerrainComponent::set_brush_direction01(float v) {
  v = clamp01(v);
  auto last_frac = cube_march_params.instrument_brush_circle_frac;
  if (last_frac != v) {
    if (v > last_frac) {
//      cube_march_params.need_increase_wall_height = true;
      cube_march_params.instrument_brush_circle_scale += 0.01f;
    } else {
//      cube_march_params.need_decrease_wall_height = true;
      cube_march_params.instrument_brush_circle_scale -= 0.01f;
    }
//    const float lim = 0.0125f;
    const float lim = 0.02f;
    cube_march_params.instrument_brush_circle_frac = v;
    cube_march_params.instrument_brush_circle_scale = clamp(
      cube_march_params.instrument_brush_circle_scale, -lim, lim);
  }
#if 0
  cube_march_params.instrument_brush_circle_scale = cube_march_params.wall_brush_circle_scale;
#endif
}

int DebugTerrainComponent::changed_height_direction() const {
  if (cube_march_params.need_increase_wall_height) {
    return 1;
  } else if (cube_march_params.need_decrease_wall_height) {
    return -1;
  } else {
    return 0;
  }
}

void DebugTerrainComponent::on_gui_update(const TerrainGUIUpdateResult& res) {
  auto join_res_dir = [](const std::string& p) {
    return std::string{GROVE_ASSET_DIR} + "/" + p;
  };

  if (res.geometry_file_path) {
    auto& p = res.geometry_file_path.value();
    geometry_file_path = join_res_dir(p);
  }
  if (res.image_file_path) {
    auto& p = res.image_file_path.value();
    image_file_path = join_res_dir(p);
  }
  if ((res.model_translation || res.model_scale) && debug_model_index < int(debug_tforms.size())) {
    auto* tform = debug_tforms[debug_model_index];
    auto curr = tform->get_current();
    curr.scale = res.model_scale ? res.model_scale.value() : curr.scale;
    curr.translation = res.model_translation ? res.model_translation.value() : curr.translation;
    tform->set(curr);
  }
  if (res.add_model) {
    debug_models.emplace_back();
  }
  if (res.model_index) {
    debug_model_index = res.model_index.value();
  }
  if (res.recompute_cube_march_geometry) {
    cube_march_params.need_recompute = true;
  }
  if (res.clear_cube_march_geometry) {
    cube_march_params.need_clear = true;
  }
  if (res.invert_cube_march_tool) {
    cube_march_params.invert = res.invert_cube_march_tool.value();
  }
  if (res.cube_march_editing_active) {
    cube_march_params.active = res.cube_march_editing_active.value();
  }
  if (res.cube_march_hidden) {
    cube_march_params.hidden = res.cube_march_hidden.value();
  }
  if (res.cube_march_use_wall_brush) {
    cube_march_params.use_wall_brush = res.cube_march_use_wall_brush.value();
  }
  if (res.cube_march_control_wall_brush_by_instrument) {
    cube_march_params.brush_control_by_instrument = res.cube_march_control_wall_brush_by_instrument.value();
  }
  if (res.cube_march_draw_bounds) {
    cube_march_params.draw_bounds = res.cube_march_draw_bounds.value();
  }
  if (res.cube_march_editor_radius) {
    global_data.sphere_r = res.cube_march_editor_radius.value();
  }
  if (res.need_increase_cube_march_wall_height) {
    cube_march_params.need_increase_wall_height = true;
  }
  if (res.need_decrease_cube_march_wall_height) {
    cube_march_params.need_decrease_wall_height = true;
  }
  if (res.need_reinitialize_cube_march_wall) {
    cube_march_params.need_initialize_wall = true;
  }
  if (res.allow_cube_march_wall_recede) {
    cube_march_params.allow_wall_recede = res.allow_cube_march_wall_recede.value();
    global_data.debug_wall_brush.can_recede = res.allow_cube_march_wall_recede.value();
  }
  if (res.cube_march_wall_brush_speed) {
    cube_march_params.wall_brush_speed = res.cube_march_wall_brush_speed.value();
  }
  if (res.cube_march_wall_random_axis_weight) {
    cube_march_params.wall_random_axis_weight = res.cube_march_wall_random_axis_weight.value();
  }
  if (res.cube_march_wall_circle_scale) {
    cube_march_params.wall_brush_circle_scale = res.cube_march_wall_circle_scale.value();
  }

  if (res.splotch_image_file_path) {
    auto& p = res.splotch_image_file_path.value();
    splotch_image_file_path = join_res_dir(p);
  }
  if (res.ground_color_image_file_path) {
    auto& p = res.ground_color_image_file_path.value();
    color_image_file_path = join_res_dir(p);
  }

  if (res.recompute_mesh_projected_bounds) {
    place_on_mesh_params.need_recompute = true;
  }
  if (res.mesh_obb3_size) {
    place_on_mesh_params.obb3_size = res.mesh_obb3_size.value();
  }
  if (res.draw_place_on_mesh_result) {
    place_on_mesh_params.draw_result = res.draw_place_on_mesh_result.value();
  }
  if (res.place_on_mesh_normal_y_threshold) {
    place_on_mesh_params.normal_y_threshold = res.place_on_mesh_normal_y_threshold.value();
  }

  if (res.debug_roots_rotation) {
    global_data.roots_rot = res.debug_roots_rotation.value();
  }
  if (res.keep_ith_axis) {
    nodes_through_terrain_params.keep_ith_axis = res.keep_ith_axis.value();
    nodes_through_terrain_params.need_update_roots_drawable = true;
  }
  if (res.keep_axis) {
    nodes_through_terrain_params.keep_axis = res.keep_axis.value();
    nodes_through_terrain_params.need_update_roots_drawable = true;
  }
}

GROVE_NAMESPACE_END
