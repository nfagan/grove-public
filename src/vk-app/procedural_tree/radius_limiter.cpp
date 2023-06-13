#include "radius_limiter.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/SlotLists.hpp"
#include "grove/math/bounds.hpp"
#include "grove/math/intersect.hpp"
#include "grove/math/util.hpp"
#include "grove/math/GridIterator3.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace bounds;

std::atomic<uint16_t> next_radius_limiter_element_tag{1};
std::atomic<uint32_t> next_radius_limiter_aggregate_id{1};

struct RadiusLimiterElements {
  int acquire() {
    if (!free_elements.empty()) {
      auto res = free_elements.back();
      free_elements.pop_back();
      return res;
    } else {
      const auto res = int(elements.size());
      elements.emplace_back();
      return res;
    }
  }

  void release(int index) {
    assert(std::find(free_elements.begin(), free_elements.end(), index) == free_elements.end());
    free_elements.push_back(index);
  }

  std::vector<RadiusLimiterElement> elements;
  std::vector<int> free_elements;
};

struct GridCellIndices {
  struct Key {
    friend inline bool operator==(const Key& a, const Key& b) {
      return a.i == b.i;
    }

    Vec3<int16_t> i;
  };

  struct HashKey {
    size_t operator()(const Key& key) const noexcept {
      return size_t(key.i.x ^ key.i.y ^ key.i.z);
    }
  };

  std::unordered_map<Key, int, HashKey> cell_indices;
  std::vector<int> free;
};

GridCellIndices::Key make_key(int16_t i, int16_t j, int16_t k) {
  GridCellIndices::Key res;
  res.i = Vec3<int16_t>{i, j, k};
  return res;
}

Optional<int> find(const GridCellIndices& inds, GridCellIndices::Key key) {
  auto it = inds.cell_indices.find(key);
  return it == inds.cell_indices.end() ? NullOpt{} : Optional<int>(it->second);
}

int require(GridCellIndices& inds, GridCellIndices::Key key, int size, bool* is_new) {
  auto it = inds.cell_indices.find(key);
  if (it != inds.cell_indices.end()) {
    *is_new = false;
    return it->second;

  } else if (!inds.free.empty()) {
    const int res = inds.free.back();
    inds.free.pop_back();
    inds.cell_indices[key] = res;
    *is_new = false;
    return res;

  } else {
    const int res = size;
    inds.cell_indices[key] = res;
    *is_new = true;
    return res;
  }
}

[[maybe_unused]] void release(GridCellIndices& inds, GridCellIndices::Key key, int index) {
  assert(inds.cell_indices.count(key) > 0);
  assert(std::find(inds.free.begin(), inds.free.end(), index) == inds.free.end());
  inds.cell_indices.erase(key);
  inds.free.push_back(index);
}

Vec3f to_float(const Vec3<int16_t>& pow2_cell_dims) {
  Vec3f r;
  for (int i = 0; i < 3; i++) {
    r[i] = std::pow(2.0f, float(pow2_cell_dims[i]));
  }
  return r;
}

Bounds3<int16_t> cell_index_span(const Bounds3f& aabb, const Vec3<int16_t>& pow2_cell_dims) {
  assert(aabb.max.x > aabb.min.x && aabb.max.y > aabb.min.y && aabb.max.z > aabb.min.z);
  auto dims = to_float(pow2_cell_dims);
  auto p0 = floor(aabb.min / dims);
  auto p1 = floor(aabb.max / dims);
  auto p1_eq = p1 * dims;
  Vec3<int16_t> p1_off{p1_eq.x == aabb.max.x, p1_eq.y == aabb.max.y, p1_eq.z == aabb.max.z};
  Vec3<int16_t> p1i{int16_t(p1.x), int16_t(p1.y), int16_t(p1.z)};
  return Bounds3<int16_t>{
    Vec3<int16_t>{int16_t(p0.x), int16_t(p0.y), int16_t(p0.z)},
    p1i - p1_off
  };
}

} //  anon

struct bounds::RadiusLimiter {
  struct Cell {
    SlotLists<int>::List index_list;
  };

  Vec3<int16_t> pow2_cell_dims{};
  float expand_factor{};

  RadiusLimiterElements elements;
  SlotLists<int> element_indices;

  std::vector<Cell> cells;
  GridCellIndices cell_indices;
};

namespace {

void insert_index(RadiusLimiter* lim, const GridCellIndices::Key& key, int el_index) {
  bool is_new{};
  int ind = require(lim->cell_indices, key, int(lim->cells.size()), &is_new);
  if (is_new) {
    lim->cells.emplace_back();
  }
  auto& cell = lim->cells[ind];
  cell.index_list = lim->element_indices.insert(cell.index_list, el_index);
}

bool erase_index(RadiusLimiter* lim, RadiusLimiter::Cell& cell, int el_index) {
  auto it = lim->element_indices.begin(cell.index_list);
  bool found{};
  for (; it != lim->element_indices.end(); ++it) {
    if (*it == el_index) {
      lim->element_indices.erase(&cell.index_list, it);
      found = true;
      break;
    }
  }
  assert(found);
  return cell.index_list.empty();
}

void remove_index(RadiusLimiter* lim, const GridCellIndices::Key& key, int el_index) {
  auto cell_it = find(lim->cell_indices, key);
  assert(cell_it.has_value() && cell_it.value() < int(lim->cells.size()));
  auto& cell = lim->cells[cell_it.value()];
  if (erase_index(lim, cell, el_index)) {
    release(lim->cell_indices, key, cell_it.value());
  }
}

void assert_no_duplicates(const RadiusLimiter* lim) {
  std::unordered_set<int> set;
  for (auto& cell : lim->cells) {
    auto it = lim->element_indices.cbegin(cell.index_list);
    auto end = lim->element_indices.cend();
    for (; it != end; ++it) {
      assert(set.count(*it) == 0);
      set.insert(*it);
    }
    set.clear();
  }
}

[[maybe_unused]] void assert_element_present(const RadiusLimiter* lim, const Vec3<int16_t>& beg,
                                             const Vec3<int16_t>& end, int el_index) {
  for (auto it = begin_it(beg, end); is_valid(it); ++it) {
    auto key = GridCellIndices::Key{*it};
    auto cell_it = find(lim->cell_indices, key);
    assert(cell_it.has_value());
    auto& cell = lim->cells[cell_it.value()];
    auto ind_it = lim->element_indices.cbegin(cell.index_list);
    bool found{};
    for (; ind_it != lim->element_indices.cend(); ++ind_it) {
      if (*ind_it == el_index) {
        found = true;
        break;
      }
    }
    assert(found);
    (void) found;
  }
}

[[maybe_unused]] void assert_element_removed(const RadiusLimiter* lim, int el_index) {
  for (auto& cell : lim->cells) {
    auto it = lim->element_indices.cbegin(cell.index_list);
    auto end = lim->element_indices.cend();
    for (; it != end; ++it) {
      assert(*it != el_index);
      (void) el_index;
    }
  }
  auto& elements = lim->elements.free_elements;
  assert(std::find(elements.begin(), elements.end(), el_index) != elements.end());
  (void) elements;
}

Bounds3<int16_t> cell_index_span(const RadiusLimiter* lim, const OBB3f& obb) {
  return cell_index_span(obb3_to_aabb(obb), lim->pow2_cell_dims);
}

auto cbegin_it(const RadiusLimiter* lim, int16_t i, int16_t j, int16_t k) {
  if (auto ind = find(lim->cell_indices, make_key(i, j, k))) {
    auto& cell = lim->cells[ind.value()];
    return lim->element_indices.cbegin(cell.index_list);
  } else {
    return lim->element_indices.cend();
  }
}

auto cbegin_it(const RadiusLimiter* lim, const Vec3<int>& ijk) {
  constexpr int16_t min = std::numeric_limits<int16_t>::min();
  constexpr int16_t max = std::numeric_limits<int16_t>::max();
  (void) min;
  (void) max;
  assert(all(ge(ijk, Vec3<int>{min})) && all(le(ijk, Vec3<int>{max})));
  return cbegin_it(lim, int16_t(ijk.x), int16_t(ijk.y), int16_t(ijk.z));
}

int to_linear_index(int16_t i, int16_t j, int16_t k, const Vec3<int16_t>& counts) {
  const int page_offset = k * counts.x * counts.y;
  const int tile_offset = i * counts.y + j;
  return page_offset + tile_offset;
}

void accumulate(const Bounds3<int16_t>& region, const Vec3<int16_t>& counts, int* freqs) {
  assert(region.min[0] >= 0 && region.min[1] >= 0 && region.min[2] >= 0);
  assert(region.max[0] < counts[0] && region.max[1] < counts[1] && region.max[2] < counts[2]);

  auto p0 = max(Vec3<int16_t>{}, region.min);
  auto p1 = min(counts - int16_t(1), region.max);

  for (int16_t kk = p0.z; kk <= p1.z; kk++) {
    for (int16_t ii = p0.x; ii <= p1.x; ii++) {
      for (int16_t jj = p0.y; jj <= p1.y; jj++) {
        freqs[to_linear_index(ii, jj, kk, counts)]++;
      }
    }
  }
}

int gather_intersecting_impl(const RadiusLimiter* lim, const OBB3f& el_obb,
                             std::vector<RadiusLimiterElement>& out) {
  const auto span = cell_index_span(lim, el_obb);
  auto grid_it = begin_it(span.min, span.max + int16_t(1));

  std::unordered_set<int> visited;
  int num_inserted{};

  for (; is_valid(grid_it); ++grid_it) {
    auto& key = *grid_it;
    auto it = cbegin_it(lim, key.x, key.y, key.z);
    auto end = lim->element_indices.cend();
    for (; it != end; ++it) {
      const int ind = *it;
      if (visited.count(ind) > 0) {
        continue;
      } else {
        visited.insert(ind);
      }

      const auto& query_el = lim->elements.elements[ind];
      auto query_obb = query_el.to_obb(query_el.radius);
      if (obb_obb_intersect(el_obb, query_obb)) {
        out.push_back(query_el);
        num_inserted++;
      }
    }
  }

  return num_inserted;
}

} //  anon

RadiusLimiter* bounds::create_radius_limiter() {
  auto* res = new RadiusLimiter();
  res->pow2_cell_dims = Vec3<int16_t>(3);
  res->expand_factor = 2.0f;
  return res;
}

void bounds::destroy_radius_limiter(RadiusLimiter** lim) {
  delete *lim;
  *lim = nullptr;
}

bool bounds::intersects_other(const RadiusLimiter* lim, RadiusLimiterElement el) {
  const auto el_obb = el.to_obb(el.radius);
  const auto span = cell_index_span(lim, el_obb);
  const auto span_end = span.max + int16_t(1);

  for (int16_t i = span.min.x; i < span_end.x; i++) {
    for (int16_t j = span.min.y; j < span_end.y; j++) {
      for (int16_t k = span.min.z; k < span_end.z; k++) {
        auto it = cbegin_it(lim, i, j, k);
        auto end = lim->element_indices.cend();
        for (; it != end; ++it) {
          auto& query_el = lim->elements.elements[*it];
          if (query_el.aggregate_id != el.aggregate_id) {
            auto query_obb = query_el.to_obb(query_el.radius);
            if (obb_obb_intersect(el_obb, query_obb)) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

bool bounds::intersects_other_tag(const RadiusLimiter* lim, const OBB3f& el_obb,
                                  RadiusLimiterElementTag tag) {
  const auto span = cell_index_span(lim, el_obb);
  auto grid_it = begin_it(span.min, span.max + int16_t(1));

  for (; is_valid(grid_it); ++grid_it) {
    auto& key = *grid_it;
    auto it = cbegin_it(lim, key.x, key.y, key.z);
    auto end = lim->element_indices.cend();
    for (; it != end; ++it) {
      auto& query_el = lim->elements.elements[*it];
      if (query_el.tag != tag) {
        continue;
      }
      auto query_obb = query_el.to_obb(query_el.radius);
      if (obb_obb_intersect(el_obb, query_obb)) {
        return true;
      }
    }
  }

  return false;
}

int bounds::gather_intersecting(const RadiusLimiter* lim, const OBB3f& el_obb,
                                std::vector<RadiusLimiterElement>& out) {
  return gather_intersecting_impl(lim, el_obb, out);
}

int bounds::gather_intersecting(const RadiusLimiter* lim, RadiusLimiterElement el,
                                std::vector<RadiusLimiterElement>& out) {
  return gather_intersecting(lim, el.to_obb(el.radius), out);
}

int bounds::gather_intersecting_line(const RadiusLimiter* lim, const Vec3f& p0, const Vec3f& p1,
                                     std::vector<const RadiusLimiterElement*>& out) {
  /*
   * https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3443&rep=rep1&type=pdf
   */
  auto rd = p1 - p0;
  auto rl = rd.length();
  if (rl == 0.0f) {
    //  @TODO: Check if inside bounds?
    return 0;
  } else {
    rd /= rl;
  }

  const auto cell_dim = to_float(lim->pow2_cell_dims);
  Vec3<int> ro_index = to_vec3i(floor(p0 / cell_dim));

  Vec3<int> ss{rd.x > 0.0f ? 1 : rd.x == 0.0f ? 0 : -1,
               rd.y > 0.0f ? 1 : rd.y == 0.0f ? 0 : -1,
               rd.z > 0.0f ? 1 : rd.z == 0.0f ? 0 : -1};

  Vec3<int> incr{rd.x > 0.0f ? ro_index.x + 1 : ro_index.x,
                 rd.y > 0.0f ? ro_index.y + 1 : ro_index.y,
                 rd.z > 0.0f ? ro_index.z + 1 : ro_index.z};

  const auto ts = abs(cell_dim / rd);

  const auto bounds = to_vec3f(incr) * cell_dim;
  auto cs = (bounds - p0) / rd;
  cs = Vec3f{rd.x == 0.0f ? infinityf() : cs.x,
             rd.y == 0.0f ? infinityf() : cs.y,
             rd.z == 0.0f ? infinityf() : cs.z};

  int num_inserted{};
  std::unordered_set<int> visited;

  Vec3<int> is{};
  while (true) {
    Vec3<int> curr_index = ro_index + is;
    if (any(gt(curr_index, Vec3<int>{std::numeric_limits<int16_t>::max()})) ||
        any(lt(curr_index, Vec3<int>{std::numeric_limits<int16_t>::min()}))) {
      break;
    }

    auto step_beg = to_vec3f(curr_index) * cell_dim;
    if ((step_beg - p0).length() >= rl + cell_dim.length()) {
      break;
    }

    auto it = cbegin_it(lim, curr_index);
    for (; it != lim->element_indices.cend(); ++it) {
      const int element_index = *it;
      if (visited.count(element_index) > 0) {
        continue;
      }

      auto& element = lim->elements.elements[element_index];
      auto element_bounds = element.to_obb(element.radius);

      float t0;
      float t1;
      if (ray_obb_intersect(p0, rd, element_bounds, &t0, &t1) && t0 > 0.0f && t0 <= rl) {
        visited.insert(element_index);
        out.push_back(&element);
        num_inserted++;
      }
    }

    if (cs.x < cs.y && cs.x < cs.z) {
      assert(std::isfinite(ts.x));
      is.x += ss.x;
      cs.x += ts.x;
    } else if (cs.y < cs.z) {
      assert(std::isfinite(ts.y));
      is.y += ss.y;
      cs.y += ts.y;
    } else {
      assert(std::isfinite(ts.z));
      is.z += ss.z;
      cs.z += ts.z;
    }
  }

  return num_inserted;
}

RadiusLimiterElementHandle bounds::insert(RadiusLimiter* lim, RadiusLimiterElement el,
                                          bool pad_radius) {
  assert(el.tag.tag > 0);
  assert(el.aggregate_id.id > 0);
  assert(el.radius > 0.0f);
  assert(el.half_length > 0.0f);

  if (pad_radius) {
    el.radius *= lim->expand_factor;
  }

  auto span = cell_index_span(lim, el.to_obb(el.radius));
  auto span_end = span.max + int16_t(1);

  const int el_index = lim->elements.acquire();
  lim->elements.elements[el_index] = el;

  for (int16_t i = span.min.x; i < span_end.x; i++) {
    for (int16_t j = span.min.y; j < span_end.y; j++) {
      for (int16_t k = span.min.z; k < span_end.z; k++) {
        insert_index(lim, make_key(i, j, k), el_index);
      }
    }
  }

  return RadiusLimiterElementHandle{el_index};
}

void bounds::remove(RadiusLimiter* lim, RadiusLimiterElementHandle el) {
  assert(el != RadiusLimiterElementHandle::invalid());
  const auto* targ_el = read_element(lim, el);
  auto obb = targ_el->to_obb(targ_el->radius);
  auto span = cell_index_span(lim, obb);
  auto grid_it = begin_it(span.min, span.max + int16_t(1));
  for (; is_valid(grid_it); ++grid_it) {
    remove_index(lim, GridCellIndices::Key{*grid_it}, el.index);
  }
  lim->elements.release(el.index);
#ifdef GROVE_DEBUG
  assert_element_removed(lim, el.index);
#endif
}

float bounds::expand(RadiusLimiter* lim, RadiusLimiterElementHandle handle, float target_radius) {
  assert(handle != RadiusLimiterElementHandle::invalid());

  const int element_index = handle.index;
  auto& el = lim->elements.elements[element_index];
  if (el.radius >= target_radius || el.reached_maximum_radius) {
    return std::min(el.radius, target_radius);
  }

  auto curr_span = cell_index_span(lim, el.to_obb(el.radius));
  auto curr_span_end = curr_span.max + int16_t(1);

  float expand = lim->expand_factor;
  assert(expand >= 1.0f);
  auto new_obb = el.to_obb(target_radius * expand);
  auto new_span = cell_index_span(lim, new_obb);
  auto new_span_end = new_span.max + int16_t(1);

  for (int16_t i = new_span.min.x; i < new_span_end.x; i++) {
    for (int16_t j = new_span.min.y; j < new_span_end.y; j++) {
      for (int16_t k = new_span.min.z; k < new_span_end.z; k++) {
        auto it = cbegin_it(lim, i, j, k);
        auto end = lim->element_indices.cend();
        for (; it != end; ++it) {
          auto& query_el = lim->elements.elements[*it];
          if (query_el.aggregate_id == el.aggregate_id) {
            //  Allow intersections between nodes that are part of the same aggregate.
            continue;
          }
          auto query_obb = query_el.to_obb(query_el.radius);
          int step{};
          while (expand > 1.0f && step < 32 && obb_obb_intersect(query_obb, new_obb)) {
            expand = lerp(0.5f, 1.0f, expand);
            new_obb = el.to_obb(target_radius * expand);
            ++step;
          }
        }
      }
    }
  }

  el.radius = target_radius * expand;
  if (expand < lim->expand_factor) {
    el.reached_maximum_radius = true;
  }

  assert(new_obb == el.to_obb(el.radius));
  new_span = cell_index_span(lim, new_obb);
  new_span_end = new_span.max + int16_t(1);

  if (new_span != curr_span) {
    for (int16_t i = new_span.min.x; i < new_span_end.x; i++) {
      for (int16_t j = new_span.min.y; j < new_span_end.y; j++) {
        for (int16_t k = new_span.min.z; k < new_span_end.z; k++) {
          auto key = make_key(i, j, k);
          bool is_new{};
          for (int h = 0; h < 3; h++) {
            if (key.i[h] < curr_span.min[h] || key.i[h] >= curr_span_end[h]) {
              is_new = true;
              break;
            }
          }
          if (is_new) {
            insert_index(lim, key, element_index);
          }
        }
      }
    }
  }

#ifdef GROVE_DEBUG
  assert_element_present(lim, new_span.min, new_span_end, element_index);
#endif

  return std::min(el.radius, target_radius);
}

const RadiusLimiterElement* bounds::read_element(const RadiusLimiter* lim,
                                                 RadiusLimiterElementHandle elem) {
  assert(elem != RadiusLimiterElementHandle::invalid());
  assert(elem.index < int(lim->elements.elements.size()));
  return lim->elements.elements.data() + elem.index;
}

void bounds::filter_histogram(const int* freqs, const Vec3<int16_t>& counts,
                              float* tmp, float* out) {
  for (int16_t k = 0; k < counts.z; k++) {
    for (int16_t i = 0; i < counts.x; i++) {
      for (int16_t j = 0; j < counts.y; j++) {
        int16_t k0 = int16_t(std::max(0, k - 1));
        int16_t k1 = std::min(int16_t(counts.z - 1), k);
        int ind0 = to_linear_index(i, j, k0, counts);
        int ind = to_linear_index(i, j, k, counts);
        int ind1 = to_linear_index(i, j, k1, counts);
        out[ind] = float(freqs[ind0] + freqs[ind] + freqs[ind1]) / 3.0f;
      }
    }
  }
  for (int16_t k = 0; k < counts.z; k++) {
    for (int16_t i = 0; i < counts.x; i++) {
      for (int16_t j = 0; j < counts.y; j++) {
        int16_t j0 = int16_t(std::max(0, j - 1));
        int16_t j1 = std::min(int16_t(counts.y - 1), j);
        int ind0 = to_linear_index(i, j0, k, counts);
        int ind = to_linear_index(i, j, k, counts);
        int ind1 = to_linear_index(i, j1, k, counts);
        tmp[ind] = (out[ind0] + out[ind] + out[ind1]) / 3.0f;
      }
    }
  }
  for (int16_t k = 0; k < counts.z; k++) {
    for (int16_t i = 0; i < counts.x; i++) {
      for (int16_t j = 0; j < counts.y; j++) {
        int16_t i0 = int16_t(std::max(0, i - 1));
        int16_t i1 = std::min(int16_t(counts.x - 1), i);
        int ind0 = to_linear_index(i0, j, k, counts);
        int ind = to_linear_index(i, j, k, counts);
        int ind1 = to_linear_index(i1, j, k, counts);
        out[ind] = (tmp[ind0] + tmp[ind] + tmp[ind1]) / 3.0f;
      }
    }
  }
}

#define USE_CENTER_WEIGHT (0)

Vec3f bounds::mean_gradient(const float* hist, const Vec3<int16_t>& cell_counts) {
  Vec3<double> sum{};
#if USE_CENTER_WEIGHT
  Vec3<double> ct{};
  auto ct_half = to_vec3f(cell_counts / int16_t(2));
#else
  double ct{};
#endif

  for (int16_t k = 0; k < cell_counts.z-1; k++) {
    for (int16_t i = 0; i < cell_counts.x-1; i++) {
      for (int16_t j = 0; j < cell_counts.y-1; j++) {
        const int ind = to_linear_index(i, j, k, cell_counts);
        const float cs = hist[ind];

#if USE_CENTER_WEIGHT
        auto to_center = abs((to_vec3f(Vec3<int16_t>{i, j, k}) + 0.5f) - ct_half);
        auto atten = 1.0f - max(Vec3f{}, min(to_center, ct_half)) / ct_half;
#endif
        int ind_next;
        ind_next = to_linear_index(int16_t(i + 1), j, k, cell_counts);
        float dx = hist[ind_next] - cs;
#if USE_CENTER_WEIGHT
        dx *= atten.x;
#endif
        sum.x += dx;

        ind_next = to_linear_index(i, int16_t(j + 1), k, cell_counts);
        float dy = hist[ind_next] - cs;
#if USE_CENTER_WEIGHT
        dy *= atten.y;
#endif
        sum.y += dy;

        ind_next = to_linear_index(i, j, int16_t(k + 1), cell_counts);
        float dz = hist[ind_next] - cs;
#if USE_CENTER_WEIGHT
        dz *= atten.z;
#endif
        sum.z += dz;
#if USE_CENTER_WEIGHT
        ct.x += atten.x;
        ct.y += atten.y;
        ct.z += atten.z;
#else
        ct++;
#endif
      }
    }
  }
#if USE_CENTER_WEIGHT
  for (int i = 0; i < 3; i++) {
    if (ct[i] > 0.0) {
      sum[i] /= ct[i];
    }
  }
#else
  if (ct > 0.0) {
    sum /= ct;
  }
#endif

  return to_vec3f(sum);
}

void bounds::histogram(const RadiusLimiter* lim, const Vec3<int16_t>& ori,
                       const Vec3<int16_t>& pow2_cell_size, const Vec3<int16_t>& cell_counts,
                       uint32_t aggregate, int* freqs) {
  const auto cell_size_float = to_float(pow2_cell_size);
  auto ori_float = to_vec3f(ori) * cell_size_float;
  auto size_float = to_vec3f(cell_counts) * cell_size_float;
  auto bounds = Bounds3f{ori_float, ori_float + size_float};
  auto lim_span = cell_index_span(bounds, lim->pow2_cell_dims);

  Bounds3<int16_t> hist_region{ori, ori + cell_counts - int16_t(1)};
  for (int16_t i = lim_span.min.x; i <= lim_span.max.x; i++) {
    for (int16_t j = lim_span.min.y; j <= lim_span.max.y; j++) {
      for (int16_t k = lim_span.min.z; k <= lim_span.max.z; k++) {
        auto it = cbegin_it(lim, i, j, k);
        auto end = lim->element_indices.cend();
        for (; it != end; ++it) {
          auto& query_el = lim->elements.elements[*it];
          if (query_el.aggregate_id.id == aggregate) {
            continue;
          }

          auto query_bounds = obb3_to_aabb(query_el.to_obb(query_el.radius));
          auto bounds_region = cell_index_span(query_bounds, pow2_cell_size);
          auto grid_region = intersect_of(hist_region, bounds_region);

          grid_region.min -= ori;
          grid_region.max -= ori;
          accumulate(grid_region, cell_counts, freqs);
        }
      }
    }
  }
}

RadiusLimiterStats bounds::get_stats(const RadiusLimiter* lim) {
  RadiusLimiterStats result{};
  result.num_cells = int(lim->cells.size());
  result.num_cell_indices = int(lim->cell_indices.cell_indices.size());
  result.num_free_cell_indices = int(lim->cell_indices.free.size());
  result.num_elements = int(lim->elements.elements.size());
  result.num_free_elements = int(lim->elements.free_elements.size());
  result.num_element_indices = int(lim->element_indices.num_nodes());
  result.num_free_element_indices = int(lim->element_indices.num_free_nodes());
  return result;
}

void bounds::validate(const RadiusLimiter* lim) {
  assert_no_duplicates(lim);
}

bounds::RadiusLimiterElementTag bounds::RadiusLimiterElementTag::create() {
  RadiusLimiterElementTag result;
  result.tag = next_radius_limiter_element_tag++;
  return result;
}

RadiusLimiterAggregateID bounds::RadiusLimiterAggregateID::create() {
  RadiusLimiterAggregateID result;
  result.id = next_radius_limiter_aggregate_id++;
  return result;
}

RadiusLimiterElement RadiusLimiterElement::create_enclosing_obb3(
  const OBB3f& bounds, RadiusLimiterAggregateID id, RadiusLimiterElementTag tag) {
  //
  RadiusLimiterElement result{};
  result.i = bounds.i;
  result.j = bounds.j;
  result.k = bounds.k;
  result.p = bounds.position;
  result.half_length = bounds.half_size.y;
  result.radius = std::max(bounds.half_size.x, bounds.half_size.z);
  result.aggregate_id = id;
  result.tag = tag;
  return result;
}

GROVE_NAMESPACE_END
