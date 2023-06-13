#include "../../attraction_points.hpp"
#include "../../components.hpp"
#include "grove/math/random.hpp"
#include <chrono>
#include <iostream>

using namespace grove;
using namespace grove::tree;

namespace {

struct Config {
  static constexpr float initial_span_size = 512.0f;
  static constexpr float max_span_size_split = 4.0f;
};

struct ExampleData {
  Vec3f position{};
  bool active{};
};

struct ExampleDataTraits {
  static inline auto position(const ExampleData& data) {
    return data.position;
  }
  static inline bool empty(const ExampleData& data) {
    return !data.active;
  }
  static inline void clear(ExampleData& data) {
    data.active = false;
  }
  static inline void fill(ExampleData& data) {
    data.active = true;
  }
};

void profile_example() {
  float initial_span_size{1024.0f};
  float max_span_size_split{1.0f};

  PointOctree<ExampleData, ExampleDataTraits> oct{initial_span_size, max_span_size_split};
  const auto num_pts = int(1e4);
  float radius = 4.0f;
  int num_added{};
  auto pts = points::uniform_sphere(num_pts, Vec3f{radius});
  auto pts2 = points::uniform_sphere(num_pts, Vec3f{radius}, Vec3f{8.0f, 0.0f, 0.0f});

  auto t0 = std::chrono::high_resolution_clock::now();
  for (auto& p : pts) {
    if (oct.insert(p, {p, true})) {
      num_added++;
    }
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  auto dur_ms0 = std::chrono::duration<double>(t1 - t0).count() * 1e3;

  t0 = std::chrono::high_resolution_clock::now();
  for (auto& p : pts2) {
    if (oct.insert(p, {p, true})) {
      num_added++;
    }
  }
  t1 = std::chrono::high_resolution_clock::now();
  auto dur_ms1 = std::chrono::duration<double>(t1 - t0).count() * 1e3;

  std::cout << "Num added: " << num_added
            << "; Num requested: " << pts.size() + pts2.size()
            << "; Num non-empty: " << oct.count_non_empty()
            << "; Num nodes: " << oct.num_nodes()
            << "; Approx mem kb: " << double(oct.num_nodes() * sizeof(ExampleData)) / 1024.0
            << "; first: " << dur_ms0 << "ms" << "; second: " << dur_ms1 << "ms"
            << std::endl;

  auto c = points::uniform_sphere() * 4.0f;
  auto r = 1.0f;
  std::vector<decltype(oct)::Node*> nodes;

  t0 = std::chrono::high_resolution_clock::now();
  oct.collect_within_sphere(nodes, c, r);
  t1 = std::chrono::high_resolution_clock::now();
  auto dur_ms_collect = std::chrono::duration<double>(t1 - t0).count() * 1e3;

  std::cout << "Num collected: " << nodes.size()
            << "; " << dur_ms_collect << "ms"
            << std::endl;
}

AttractionPoints make_default_oct() {
  return AttractionPoints{Config::initial_span_size, Config::max_span_size_split};
}

std::vector<Vec3f>
high_above_ground_attraction_points(int count, const Vec3f& ori, float tree_scale) {
  std::vector<Vec3f> result(count);
  auto scl = Vec3f{2.0f, 4.0f, 2.0f} * tree_scale;
  points::uniform_cylinder_to_hemisphere(result.data(), count, scl, ori);
  return result;
}

void test_duplicates() {
  const float max_span_size_split = 4.0f;
  AttractionPoints oct{512.0f, max_span_size_split};

  const auto pts = points::uniform_cylinder_to_hemisphere(1, Vec3f{2.0f, 4.0f, 2.0f} * 10.0f);
  int num_added{};
  for (int i = 0; i < 2; i++) {
    if (oct.insert(pts[0], tree::make_attraction_point(pts[0], 1u))) {
      num_added++;
    }
  }

  oct.validate();
  assert(num_added == 1);
}

void test_reinsert() {
  const float max_span_size_split = 4.0f;
  const float root_span = 512.0f;
  AttractionPoints oct{root_span, max_span_size_split};

  const Vec3f global_scale{10.0f};
  const auto scl = Vec3f{2.0f, 4.0f, 2.0f} * global_scale;
  auto pts = points::uniform_cylinder_to_hemisphere(10000, scl);

  std::vector<Vec3f> added_points;
  for (auto& p : pts) {
    if (oct.insert(p, tree::make_attraction_point(p, 1u))) {
      added_points.push_back(p);
    }
  }

  std::cout << "Num added: " << added_points.size()
            << " Num nodes: " << oct.num_nodes() << std::endl;
  oct.validate();

  size_t num_active{};
  for (const auto& node : oct.read_nodes()) {
    if (node.data.is_active()) {
      num_active++;
    }
  }
  assert(num_active == added_points.size() && num_active == oct.count_non_empty());

  int pt_ind{};
  auto to_reinsert = added_points;
  for (auto& p : to_reinsert) {
    assert(!oct.insert(p, tree::make_attraction_point(p, 1u)));
    pt_ind++;
  }

  for (auto& p : to_reinsert) {
    assert(oct.clear(p));
  }
  assert(oct.count_non_empty() == 0);

  auto num_nodes = oct.num_nodes();
  for (auto& p : to_reinsert) {
    assert(oct.insert(p, tree::make_attraction_point(p, 1u)));
  }
  assert(oct.num_nodes() == num_nodes);

  auto to_shuffle = added_points;
  std::sort(to_shuffle.begin(), to_shuffle.end(), Vec3f::Less{});
  AttractionPoints oct2{root_span, max_span_size_split};
  for (auto& p : to_shuffle) {
    assert(oct2.insert(p, tree::make_attraction_point(p, 1u)));
  }

#if 0
  for (uint8_t i = 0; i < 8; i++) {
    const uint8_t i0 = i & uint8_t(1);
    const uint8_t i1 = (i >> uint8_t(1)) & uint8_t(1);
    const uint8_t i2 = (i >> uint8_t(2)) & uint8_t(1);
    printf("%d, %d, %d\n", i0, i1, i2);
  }
#endif
}

Vec3f random_tree_origin(Vec3f ori) {
  return ori + Vec3f{urand_11f(), 0.0f, urand_11f()} * 64.0f;
}

void profile_several_origins() {
  using Clock = std::chrono::high_resolution_clock;
  using Duration = std::chrono::duration<double>;

  const Vec3f origin{32.0f, 0.0f, -32.0f};
  const auto scl = Vec3f{2.0f, 4.0f, 2.0f} * 10.0f;
  const float max_span_size_split = 4.0f;
  const float root_span = 512.0f;
  AttractionPoints oct{root_span, max_span_size_split};

  const int num_trees = 100;
  int num_to_insert{};
  int num_inserted{};
  Duration insert_time{};

  for (int i = 0; i < num_trees; i++) {
    auto num_insert = int(1e4);
    num_to_insert += num_insert;
    auto ps = points::uniform_cylinder_to_hemisphere(num_insert, scl, random_tree_origin(origin));
    auto t0 = Clock::now();
    for (auto& p : ps) {
      if (oct.insert(p, tree::make_attraction_point(p, 1u))) {
        num_inserted++;
      }
    }
    insert_time += (Clock::now() - t0);
#ifdef GROVE_DEBUG
    oct.validate();
#endif
  }

  auto elapsed_ms = insert_time.count() * 1e3;
  printf("Inserted %d attraction points (%0.2f) for %d trees in %0.2fms\n",
         num_inserted, float(num_inserted)/float(num_to_insert), num_trees, elapsed_ms);
}

std::unordered_set<int> random_ints(int count, int ub) {
  std::unordered_set<int> res;
  while (int(res.size()) != count) {
    res.insert(int(urand() * ub));
  }
  return res;
}

std::vector<int> insert_into(AttractionPoints& oct, const std::vector<Vec3f>& ps, uint32_t id) {
  int pi{};
  std::vector<int> inserted;
  for (auto& p : ps) {
    if (oct.insert(p, tree::make_attraction_point(p, id))) {
      inserted.push_back(pi);
    }
    pi++;
  }
  return inserted;
}

std::unordered_set<int> clear_subset(AttractionPoints& oct,
                                     const std::vector<int>& inserted,
                                     const std::vector<Vec3f>& ps,
                                     int max_num_clear) {
  const int num_inserted = int(inserted.size());
  auto res = random_ints(std::min(max_num_clear, num_inserted), num_inserted);
  for (int i : res) {
    bool success = oct.clear(ps[inserted[i]]);
    assert(success);
  }
  return res;
}

AttractionPoints default_rebuild(AttractionPoints&& src) {
  return AttractionPoints::rebuild_active(
    std::move(src), Config::initial_span_size, Config::max_span_size_split);
}

int assert_found(AttractionPoints& points, const std::vector<int>& inserted,
                 const std::vector<Vec3f>& ps, const std::unordered_set<int>& cleared) {
  int num_found{};
  for (int i = 0; i < int(inserted.size()); i++) {
    auto* data = points.find(ps[inserted[i]]);
    if (cleared.count(i)) {
      assert(!data);
    } else {
      assert(data);
      num_found++;
    }
  }
  return num_found;
}

void test_rebuild() {
  const int num_points = int(1e4);
  const float point_scale = 10.0f;

  auto src_oct = make_default_oct();
  auto src_pts = high_above_ground_attraction_points(num_points, {}, point_scale);

  auto inserted = insert_into(src_oct, src_pts, 1u);
  const int num_inserted = int(inserted.size());

  auto to_clear = random_ints(std::min(100, num_inserted), num_inserted);
  for (int i : to_clear) {
    bool success = src_oct.clear(src_pts[inserted[i]]);
    assert(success);
    (void) success;
  }
  {
    auto num_empty = src_oct.count_empty_leaves();
    assert(num_empty == to_clear.size());
  }

  auto store_oct = src_oct;
  auto rebuilt = default_rebuild(std::move(src_oct));
  rebuilt.validate();
  assert(rebuilt.count_empty_leaves() == 0);
  assert(rebuilt.count_non_empty_leaves() == num_inserted - to_clear.size());

  for (auto& node : store_oct.read_nodes()) {
    if (node.data.is_active()) {
      assert(rebuilt.find(node.data.position));
    }
  }

  //  Insert some new points into the rebuilt oct, and assert that all the original points
  //  can still be cleared.
  auto new_pts = high_above_ground_attraction_points(num_points, {}, point_scale);
  auto new_insert = insert_into(rebuilt, new_pts, 1u);
  rebuilt.validate();
  assert(rebuilt.count_empty_leaves() == 0);

  int num_cleared_after{};
  for (int i = 0; i < int(inserted.size()); i++) {
    bool success = rebuilt.clear(src_pts[inserted[i]]);
    if (to_clear.count(i)) {
      assert(!success);
    } else {
      assert(success);
      num_cleared_after++;
    }
  }

  //  Insert some more points after clearing.
  auto new_pts2 = high_above_ground_attraction_points(
    num_points, Vec3f{1.0f, 0.0f, 0.0f}, point_scale);
  auto new_insert2 = insert_into(rebuilt, new_pts2, 2u);
  rebuilt.validate();
  auto to_clear2 = clear_subset(rebuilt, new_insert2, new_pts2, 100);
  rebuilt.validate();
  assert_found(rebuilt, new_insert2, new_pts2, to_clear2);

  auto new_pts3 = high_above_ground_attraction_points(
    num_points, Vec3f{-1.0f, 0.0f, 0.0f}, point_scale);
  auto new_insert3 = insert_into(rebuilt, new_pts3, 3u);
  auto to_clear3 = clear_subset(rebuilt, new_insert3, new_pts3, 100);
  rebuilt.validate();

  assert_found(rebuilt, new_insert2, new_pts2, to_clear2);
  assert_found(rebuilt, new_insert3, new_pts3, to_clear3);
  auto rebuilt2 = default_rebuild(std::move(rebuilt));
  rebuilt2.validate();
  assert(rebuilt2.count_empty_leaves() == 0);

  assert_found(rebuilt2, new_insert2, new_pts2, to_clear2);
  assert_found(rebuilt2, new_insert3, new_pts3, to_clear3);

  {
    size_t num_cleared = rebuilt2.clear_if([&](const AttractionPoint* pt) {
      return pt->id() == 3u;
    });
    assert(num_cleared + to_clear3.size() == new_insert3.size());
  }
}

void test_new_attraction_point() {
  const uint32_t id = 1u << 29u;
  auto point = make_attraction_point(Vec3f{2.0f, 1.0f, 4.0f}, id);
  assert(point.is_active());
  assert(!point.is_consumed());
  assert(point.id() == id);

  point.set_consumed(true);
  assert(point.is_active());
  assert(point.is_consumed());
  assert(point.id() == id);
  point.set_consumed(false);
  assert(point.is_active() && !point.is_consumed() && point.id() == id);
  point.set_active(false);
  assert(!point.is_active() && !point.is_consumed() && point.id() == id);

  uint32_t new_id = 3u;
  point.set_id(new_id);
  assert(!point.is_active() && !point.is_consumed());
  auto curr_id = point.id();
  assert(!point.is_active() && !point.is_consumed() && curr_id == new_id);

  point.set_consumed(true);
  assert(point.id() == new_id && point.is_consumed() && !point.is_active());
  point.set_id(7u);
  assert(point.id() == 7u && point.is_consumed() && !point.is_active());

  printf("OK.\n");
}

} //  anon

int main(int, char**) {
  printf("Attraction point data is %u bytes.\n", uint32_t(sizeof(AttractionPoint)));

  test_new_attraction_point();
  test_rebuild();
  test_duplicates();
  test_reinsert();
  profile_example();
  profile_several_origins();
  return 0;
}

