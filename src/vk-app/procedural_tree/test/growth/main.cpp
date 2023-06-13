#include "vk-app/procedural_tree/growth_system.hpp"
#include "app/procedural_tree/GrowthSystem.hpp"
#include "app/procedural_tree/attraction_points.hpp"
#include "app/procedural_tree/serialize.hpp"
#include "grove/math/random.hpp"
#include <string>

using namespace grove;
using namespace grove::tree;

struct Config {
  static constexpr float initial_attraction_point_span_size = 512.0f;
  static constexpr float max_attraction_point_span_size_split = 4.0f;
  static constexpr int max_num_internodes = 512;
};

Vec3f random_tree_origin() {
  auto off = Vec3f{urand_11f(), 0.0f, urand_11f()} * 32.0f;
  return Vec3f{32.0f, 0.0f, -32.0f} + off;
}

float random_tree_scale() {
  return 10.0f + urand_11f() * 2.0f;
}

struct DebugTree {
  TreeNodeStore tree;
  SpawnInternodeParams spawn_params;
  DistributeBudQParams bud_q_params;
  std::vector<Vec3f> attraction_points;
};

std::vector<Vec3f> high_above_ground_attraction_points(int n, const Vec3f& ori, float tree_scale) {
  auto scl = Vec3f{2.0f, 4.0f, 2.0f} * tree_scale;
  return points::uniform_cylinder_to_hemisphere(n, scl, ori);
}

DebugTree make_tree(int num_points) {
  DebugTree result;
  float tree_scale = random_tree_scale();
  auto tree_ori = random_tree_origin();
  result.spawn_params = tree::SpawnInternodeParams::make_debug(tree_scale);
  result.bud_q_params = tree::DistributeBudQParams::make_debug();
  result.tree = make_tree_node_store(tree_ori, result.spawn_params);
  result.attraction_points = high_above_ground_attraction_points(num_points, tree_ori, tree_scale);
  return result;
}

void run1(std::vector<DebugTree>& trees, AttractionPoints* dst_attrac_points) {
  AttractionPoints attraction_points{
    Config::initial_attraction_point_span_size,
    Config::max_attraction_point_span_size_split
  };

  GrowthSystem growth_system;
  growth_system.initialize();

  std::vector<std::function<std::vector<Vec3f>()>> make_attrac_point_funcs;
  for (auto& tree : trees) {
    make_attrac_point_funcs.emplace_back() = [&tree]() {
      return tree.attraction_points;
    };
  }

  std::vector<GrowthSystem::GrowableTree> growables;
  for (size_t i = 0; i < trees.size(); i++) {
    auto& tree = trees[i];
    auto& growable = growables.emplace_back();
    growable.nodes = &tree.tree;
    growable.spawn_params = &tree.spawn_params;
    growable.bud_q_params = &tree.bud_q_params;
    growable.make_attraction_points = &make_attrac_point_funcs[i];
    growable.max_num_internodes = Config::max_num_internodes;
    growable.last_num_internodes = int(tree.tree.internodes.size());
  }

  grove::srand(0);
  growth_system.fill_context(&attraction_points, std::move(growables));
  growth_system.submit();

  while (true) {
    auto res = growth_system.update();
    if (res.finished_growing) {
      break;
    }
  }

  *dst_attrac_points = std::move(attraction_points);
}

void run2(std::vector<DebugTree>& trees, int num_points,
          AttractionPoints* dst_attrac_points) {
  GrowthSystem2 growth_system2;
  CreateGrowthContextParams context_params{};

  context_params.max_num_attraction_points_per_tree = num_points;
  context_params.max_attraction_point_span_size_split = Config::max_attraction_point_span_size_split;
  context_params.initial_attraction_point_span_size = Config::initial_attraction_point_span_size;
  auto ctx = create_growth_context(&growth_system2, context_params);

  std::vector<GrowthSystem2::FutureGrowthResult> futs;
  for (auto& tree : trees) {
    PrepareToGrowParams params{};
    params.context = ctx;
    params.nodes = std::move(tree.tree);
    params.spawn_params = std::move(tree.spawn_params);
    params.bud_q_params = std::move(tree.bud_q_params);
    params.make_attraction_points = [&tree, num_points](Vec3f* dst, int max_num) {
      assert(max_num == num_points && int(tree.attraction_points.size()) == max_num);
      int ct{};
      for (auto& p : tree.attraction_points) {
        dst[ct++] = p;
      }
      return ct;
    };
    params.max_num_internodes = Config::max_num_internodes;
    futs.push_back(prepare_to_grow(&growth_system2, std::move(params)));
  }

  grove::srand(0);
  grow(&growth_system2, ctx);
  while (true) {
    update(&growth_system2);
    bool all_ready = true;
    for (auto& fut : futs) {
      if (!fut->is_ready()) {
        all_ready = false;
        break;
      }
    }
    if (all_ready) {
      break;
    }
  }

  for (size_t i = 0; i < futs.size(); i++) {
    auto& dst = trees[i];
    auto& src = futs[i]->data;
    dst.spawn_params = std::move(src.spawn_params);
    dst.tree = std::move(src.nodes);
    dst.bud_q_params = std::move(src.bud_q_params);
  }

  *dst_attrac_points = std::move(growth_system2.growth_contexts[0]->attraction_points);
}

bool equal(const AttractionPoint& a, const AttractionPoint& b) {
  return a.active == b.active && a.consumed == b.consumed && a.position == b.position;
}

void assert_eq(const Internodes& nodes1, const Internodes& nodes2) {
  assert(nodes1.size() == nodes2.size());
  for (size_t j = 0; j < nodes1.size(); j++) {
    auto& a = nodes1[j];
    auto& b = nodes2[j];
//    assert(a.id == b.id); //  allow ids to differ.
    assert(a.parent == b.parent);
    assert(a.medial_child == b.medial_child);
    assert(a.lateral_child == b.lateral_child);
    assert(a.position == b.position);
    assert(a.render_position == b.render_position);
    assert(a.direction == b.direction);
    assert(a.length == b.length);
    assert(a.length_scale == b.length_scale);
    assert(a.diameter == b.diameter);
    assert(a.lateral_q == b.lateral_q);
    assert(a.bud_indices[0] == b.bud_indices[0]);
    assert(a.bud_indices[1] == b.bud_indices[1]);
    assert(a.num_buds == b.num_buds);
    assert(a.gravelius_order == b.gravelius_order);
  }
}

void test_growth_systems() {
  const int num_trees = 10;
  const int num_points = int(1e4);

  std::vector<DebugTree> trees(num_trees);
  for (auto& tree : trees) {
    tree = make_tree(num_points);
  }

  AttractionPoints attraction_points1;
  auto trees1 = trees;
  run1(trees1, &attraction_points1);

  AttractionPoints attraction_points2;
  auto trees2 = trees;
  run2(trees2, num_points, &attraction_points2);

  AttractionPoints attraction_points3;
  auto trees3 = trees;
  run1(trees3, &attraction_points3);

#if 1
  {
    std::string res_dir{GROVE_PLAYGROUND_RES_DIR};
    res_dir += "/serialized_trees/test/";
    const decltype(&trees1) ptrs[3]{&trees1, &trees2, &trees3};
    const char* names[3]{"tree1", "tree2", "tree3"};
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < num_trees; j++) {
        std::string filei{res_dir};
        filei += names[i];
        filei += "-" + std::to_string(j) + ".dat";
        (void) tree::serialize_file((*ptrs[i])[j].tree, filei.c_str());
      }
    }
  }
#endif

  auto anodes1 = attraction_points1.read_nodes();
  auto anodes2 = attraction_points2.read_nodes();
  assert(anodes1.size() == anodes2.size());
  for (int64_t i = 0; i < int64_t(anodes1.size()); i++) {
    auto& a = anodes1[i];
    auto& b = anodes2[i];
    assert(a.is_leaf() == b.is_leaf());
    assert(equal(a.data, b.data));
  }

  for (int i = 0; i < num_trees; i++) {
    auto& nodes1 = trees1[i].tree.internodes;
    auto& nodes2 = trees2[i].tree.internodes;
    auto& nodes3 = trees3[i].tree.internodes;
    assert_eq(nodes1, nodes3);  //  multiple runs of the same system.
    assert_eq(nodes1, nodes2);
  }
}

int main(int, char**) {
  test_growth_systems();
  return 0;
}