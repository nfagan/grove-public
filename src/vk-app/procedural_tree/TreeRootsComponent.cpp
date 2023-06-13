#include "TreeRootsComponent.hpp"
#include "roots_system.hpp"
#include "render_roots_system.hpp"
#include "resource_flow_along_nodes.hpp"
#include "../terrain/terrain.hpp"
#include "grove/math/random.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/common/common.hpp"
#include "grove/common/DynamicArray.hpp"
#include "grove/common/Optional.hpp"

GROVE_NAMESPACE_BEGIN

struct ResourceSpirals {
  tree::ResourceSpiralAroundNodesHandle handles[4]{};
  int count{};
};

struct RootsComponentInstance {
  Optional<tree::RootsInstanceHandle> roots_instance;
  Optional<tree::RenderRootsInstanceHandle> render_instance;
  ResourceSpirals resource_spirals;
};

struct TreeRootsComponent {
  std::vector<RootsComponentInstance> instances;
  DynamicArray<TreeRootsComponentCreateRootsParams, 2> pending_roots;
};

namespace {

using InitInfo = TreeRootsComponentInitInfo;
using UpdateInfo = TreeRootsComponentUpdateInfo;

void do_create_roots(
  TreeRootsComponent* component, const Vec3f& pos, const Vec3f& dir, const UpdateInfo& info) {
  //
  tree::CreateRootsInstanceParams roots_params{};
  roots_params.origin = pos;
  roots_params.init_direction = dir;
  auto roots = tree::create_roots_instance(info.roots_system, roots_params);

  tree::CreateRenderRootsInstanceParams render_params{};
  render_params.associated_roots = roots;
  auto render_inst = tree::create_render_roots_instance(info.render_roots_system, render_params);

  ResourceSpirals spirals{};
  for (int i = 0; i < 4; i++) {
    tree::CreateResourceSpiralParams spiral_params{};
    spiral_params.non_fixed_parent_origin = true;
    spiral_params.burrows_into_target = true;
    spiral_params.scale = 0.25f;
    spiral_params.theta_offset = float(i) * pif() * 0.1f;
    spiral_params.linear_color = Vec3<uint8_t>{255};
    spirals.handles[spirals.count++] = tree::create_resource_spiral_around_roots(
      info.resource_spiral_system, roots, spiral_params);
  }

  RootsComponentInstance roots_inst{};
  roots_inst.roots_instance = roots;
  roots_inst.render_instance = render_inst;
  roots_inst.resource_spirals = spirals;
  component->instances.push_back(roots_inst);
}

void update_instances(TreeRootsComponent* component, const UpdateInfo& info) {
  auto inst_it = component->instances.begin();
  while (inst_it != component->instances.end()) {
    auto& inst = *inst_it;
    bool need_erase{};
    if (inst.roots_instance) {
      const auto roots_inst = inst.roots_instance.value();

      if (info.can_trigger_recede && tree::can_start_dying(info.roots_system, roots_inst)) {
        tree::start_dying(info.roots_system, roots_inst);
      }

      if (tree::can_destroy_roots_instance(info.roots_system, roots_inst)) {
        tree::destroy_roots_instance(info.roots_system, roots_inst);
        if (inst.render_instance) {
          const auto render_inst = inst.render_instance.value();
          tree::destroy_render_roots_instance(info.render_roots_system, render_inst);
        }
        need_erase = true;
      }
    } else {
      need_erase = true;
    }

    if (need_erase) {
      for (int i = 0; i < inst.resource_spirals.count; i++) {
        tree::destroy_resource_spiral(info.resource_spiral_system, inst.resource_spirals.handles[i]);
      }
      inst_it = component->instances.erase(inst_it);
    } else {
      ++inst_it;
    }
  }
}

void create_pending(TreeRootsComponent* component, const UpdateInfo& info) {
  for (int i = 0; i < int(component->pending_roots.size()); i++) {
    auto& pend = component->pending_roots[i];
    const auto ori = pend.position;

    for (int j = 0; j < pend.n; j++) {
      auto theta = urand_11f() * pif();
      Mat2f rot = make_rotation(theta);
      auto off_xz = Vec2f{rot(0, 0), rot(1, 0)} * pend.r;
      auto pos = ori + Vec3f{off_xz.x, 0.0f, off_xz.y};
      if (pend.use_terrain_height) {
        pos.y = info.terrain.height_nearest_position_xz(pos);
      }
      do_create_roots(component, pos, pend.direction, info);
    }
  }

  component->pending_roots.clear();
}

void create_under_trees(TreeRootsComponent* component, const UpdateInfo& info) {
  for (int i = 0; i < info.num_newly_created_trees; i++) {
    const auto& ori = info.newly_created_tree_origins[i];
    auto add_at = ori - Vec3f{0.0f, 0.125f, 0.0f};
    const auto dir = Vec3f{0.0f, -1.0f, 0.0f};
    do_create_roots(component, add_at, dir, info);
  }
}

struct {
  TreeRootsComponent roots_component;
} globals;

} //  anon

TreeRootsComponent* get_global_tree_roots_component() {
  return &globals.roots_component;
}

void init_tree_roots_component(TreeRootsComponent*, const InitInfo& info) {
#if 0
  const int n = 8;
  const float space = 32.0f;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      float x = (float(i) / float(n - 1) * 2.0f - 1.0f) * space;
      float z = (float(j) / float(n - 1) * 2.0f - 1.0f) * space;
      const float y = -8.0f;

      tree::CreateRootsInstanceParams roots_params{};
      roots_params.make_tree = false;
      roots_params.origin = Vec3f{x, y, z};
      auto roots = tree::create_roots_instance(info.roots_system, roots_params);

      tree::CreateRenderRootsInstanceParams render_params{};
      render_params.associated_roots = roots;
      (void) tree::create_render_roots_instance(info.render_roots_system, render_params);
    }
  }
#else
  (void) info;
#endif
}

void tree_roots_component_defer_create_roots(
  TreeRootsComponent* component, const TreeRootsComponentCreateRootsParams& params) {
  //
  assert(params.n > 0);
  assert(params.direction.length() > 0.0f);
  component->pending_roots.push_back(params);
}

void tree_roots_component_simple_create_roots(
  TreeRootsComponent* component, const Vec3f& p, int n, bool up, bool use_terrain_height) {
  assert(n > 0);

  float r{};
  if (n == 1) {
    r = 0.0f;
  } else if (n < 5) {
    r = 8.0f;
  } else if (n < 10) {
    r = 16.0f;
  } else if (n < 20) {
    r = 32.0f;
  } else {
    r = 72.0f;
  }

  TreeRootsComponentCreateRootsParams params{};
  params.position = p;
  params.direction = up ? ConstVec3f::positive_y : -ConstVec3f::positive_y;
  params.use_terrain_height = use_terrain_height;
  params.n = n;
  params.r = r;
  tree_roots_component_defer_create_roots(component, params);
}

void update_tree_roots_component(TreeRootsComponent* component, const UpdateInfo& info) {
  create_under_trees(component, info);
  create_pending(component, info);
  update_instances(component, info);
}

GROVE_NAMESPACE_END
