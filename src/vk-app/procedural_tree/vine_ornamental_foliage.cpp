#include "vine_ornamental_foliage.hpp"
#include "vine_system.hpp"
#include "tree_system.hpp"
#include "../render/render_ornamental_foliage_data.hpp"
#include "../render/render_ornamental_foliage_gpu.hpp"
#include "../procedural_flower/petal.hpp"
#include "render.hpp"
#include "utility.hpp"
#include "grove/math/random.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/math/ease.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Stopwatch.hpp"
#include <vector>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree;

using UpdateInfo = VineOrnamentalFoliageUpdateInfo;

struct CreatedVineFoliageInstances {
  foliage::OrnamentalFoliageInstanceHandle instances;
  uint32_t count;
  float canonical_scale;
};

struct CurvedPlaneMaterialParams {
  int texture_layer;
  Vec3<uint8_t> color0;
  Vec3<uint8_t> color1;
  Vec3<uint8_t> color2;
  Vec3<uint8_t> color3;
};

enum class VineOrnamentalFoliageState {
  Idle = 0,
  NeedCreate,
  Growing,
  Alive,
  Dying,
  Expired
};

struct VineOrnamentalFoliageInstance {
  VineInstanceHandle associated_instance;
  VineSegmentHandle associated_segment;
  VineOrnamentalFoliageState state;
  CreatedVineFoliageInstances leaf_instances;
  CreatedVineFoliageInstances flower_instances;
  double t0;
};

struct VineOrnamentalFoliageData {
  std::vector<VineOrnamentalFoliageInstance> instances;
  Stopwatch stopwatch;
};

CurvedPlaneMaterialParams make_curved_plane_material_params(int num_texture_layers) {
  Vec3<uint8_t> color0 = Vec3<uint8_t>{255, 255, 0};
  Vec3<uint8_t> color1 = Vec3<uint8_t>{255, 255, 255};
  Vec3<uint8_t> color2 = Vec3<uint8_t>{255, 255, 77};
  Vec3<uint8_t> color3 = Vec3<uint8_t>{255, 255, 255};
#if 1
  const auto r = urandf();
  if (r < 0.25f) {
    color0 = Vec3<uint8_t>{255, 255, 0};
    color1 = Vec3<uint8_t>{255, 255, 255};
    color2 = Vec3<uint8_t>{255, 255, 77};
    color3 = Vec3<uint8_t>{255, 255, 255};
  } else if (r < 0.5f) {
    color0 = Vec3<uint8_t>{66, 175, 192};
    color1 = Vec3<uint8_t>{24, 104, 192};
    color2 = Vec3<uint8_t>{226, 28, 77};
    color3 = Vec3<uint8_t>{226, 58, 128};
  } else if (r < 0.75f) {
    color0 = Vec3<uint8_t>{255, 0, 255};
    color1 = Vec3<uint8_t>{255, 255, 0};
    color2 = Vec3<uint8_t>{255, 255, 0};
    color3 = Vec3<uint8_t>{255, 0, 255};
  } else {
    color0 = Vec3<uint8_t>{255};
    color1 = Vec3<uint8_t>{255};
    color2 = Vec3<uint8_t>{255, 255, 77};
    color3 = Vec3<uint8_t>{255, 255, 128};
  }

  const Vec3<int> ci = petal::MaterialParams::component_indices_from_perm_index(
    petal::MaterialParams::random_perm_index());
  color0 = permute(color0, ci[0], ci[1], ci[2]);
  color1 = permute(color1, ci[0], ci[1], ci[2]);
  color2 = permute(color2, ci[0], ci[1], ci[2]);
  color3 = permute(color3, ci[0], ci[1], ci[2]);
#endif
  CurvedPlaneMaterialParams result{};
  result.color0 = color0;
  result.color1 = color1;
  result.color2 = color2;
  result.color3 = color3;
  result.texture_layer = int(urandf() * float(num_texture_layers));
  return result;
}

void get_randomized_flower_params(float* dst_rs, float* dst_rp, CurvedPlaneMaterialParams* mat_desc) {
  const float rand_scale = 0.33f;

  const float rps[3] = {0.5f, 2.0f, 5.0f};
  float rp = *uniform_array_sample(rps, 3);

  const float rss[4] = {1.0f, 0.5f, 0.75f, 1.5f};
  float rs = *uniform_array_sample(rss, 4);

  rp += rp * urand_11f() * rand_scale;
  rs += rs * urand_11f() * rand_scale;

  *mat_desc = make_curved_plane_material_params(
    foliage::get_render_ornamental_foliage_num_material1_texture_layers());
  *dst_rs = rs;
  *dst_rp = rp;
}

foliage::OrnamentalFoliageInstanceDescriptor
make_vine_foliage_leaf_instance_desc(
  const Vec3f& p, const Vec3f& n, float scale, const PackedWindAxisRootInfo* branch_axis_info,
  bool is_dark) {
  //
  auto to_uint8_3 = [](const Vec3f& c) {
    using u8 = uint8_t;
    auto resf = clamp_each(c, Vec3f{}, Vec3f{1.0f}) * 255.0f;
    return Vec3<uint8_t>{u8(resf.x), u8(resf.y), u8(resf.z)};
  };

  auto rand_color = [&](const Vec3f& c, float s) {
    return to_uint8_3(c + c * Vec3f{urand_11f(), urand_11f(), urand_11f()} * s);
  };

  const Vec3f base_color0{0.145f, 0.71f, 0.155f};
  const Vec3f base_color1{};
  const Vec3f base_color2{0.681f, 0.116f, 0.0f};
  const Vec3f base_color3{0.0f, 0.623f, 0.0f};

  foliage::OrnamentalFoliageInstanceDescriptor desc{};
  desc.translation = p + n * 0.025f;
  desc.orientation = n;
  desc.material.material2.texture_layer_index = 1;  //  @TODO

  if (is_dark) {
    desc.material.material2.color0 = rand_color(Vec3f{0.145f, 0.028f, 0.07f}, 0.1f);
    desc.material.material2.color1 = {};
    desc.material.material2.color2 = {};
    desc.material.material2.color3 = rand_color(Vec3f{0.394f, 0.449f, 0.0f}, 0.1f);
  } else {
    desc.material.material2.color0 = rand_color(base_color0, 0.1f);
    desc.material.material2.color1 = rand_color(base_color1, 0.1f);
    desc.material.material2.color2 = rand_color(base_color2, 0.1f);
    desc.material.material2.color3 = rand_color(base_color3, 0.1f);
  }

  desc.geometry_descriptor.flat_plane.aspect = 1.0f;
  desc.geometry_descriptor.flat_plane.scale = scale;
  desc.geometry_descriptor.flat_plane.y_rotation_theta = urandf() * pif();

  if (branch_axis_info) {
    desc.wind_data.on_branch_axis.info0 = (*branch_axis_info)[0];
    desc.wind_data.on_branch_axis.info1 = (*branch_axis_info)[1];
    desc.wind_data.on_branch_axis.info2 = (*branch_axis_info)[2];
  } else {
    desc.wind_data.on_plant_stem.world_origin_xz = Vec2f{desc.translation.x, desc.translation.z};
    desc.wind_data.on_plant_stem.tip_y_fraction = 0.0f;
  }

  return desc;
}

foliage::OrnamentalFoliageInstanceDescriptor
make_vine_foliage_flower_instance_desc(
  const Vec3f& p, const Vec3f& n, const CurvedPlaneMaterialParams& mat,
  float radius_power, const PackedWindAxisRootInfo* axis_root_info) {
  //
  foliage::OrnamentalFoliageInstanceDescriptor desc{};
  desc.translation = p + n * 0.1f;
  desc.orientation = n;
  desc.material.material1.texture_layer_index = uint32_t(mat.texture_layer);
  desc.material.material1.color0 = mat.color0;
  desc.material.material1.color1 = mat.color1;
  desc.material.material1.color2 = mat.color2;
  desc.material.material1.color3 = mat.color3;

  desc.geometry_descriptor.curved_plane.min_radius = 0.01f;
  desc.geometry_descriptor.curved_plane.curl_scale = 0.0f;
  desc.geometry_descriptor.curved_plane.radius_power = radius_power;
//  desc.geometry_descriptor.curved_plane.radius = 1.0f;
  desc.geometry_descriptor.curved_plane.radius = 0.0f;

  if (axis_root_info) {
    desc.wind_data.on_branch_axis.info0 = (*axis_root_info)[0];
    desc.wind_data.on_branch_axis.info1 = (*axis_root_info)[1];
    desc.wind_data.on_branch_axis.info2 = (*axis_root_info)[2];
  } else {
    desc.wind_data.on_plant_stem.world_origin_xz = Vec2f{desc.translation.x, desc.translation.z};
    desc.wind_data.on_plant_stem.tip_y_fraction = 0.0f;
  }
  return desc;
}

template <typename F>
void map_along_axis(const VineNode* nodes, int beg, int end, float space, const F& func) {
  assert(end >= beg);
  if (end - beg == 0) {
    return;
  }

  Temporary<int, 1024> store_node_stack;
  int* node_stack = store_node_stack.require(end - beg);
  int si{};
  node_stack[si++] = beg;

  while (si > 0) {
    int ni = node_stack[--si];
    float len_accum{};

    while (ni != -1) {
      auto& node = nodes[ni];
      if (node.has_lateral_child()) {
        node_stack[si++] = node.lateral_child;
      }

      float len_off{};
      while (len_accum >= space) {
        len_accum -= space;
        func(node, ni, len_off);
        len_off += space;
      }

      const auto child_p =
        node.has_medial_child() ? nodes[node.medial_child].position : node.position;
      const auto node_len = (child_p - node.position).length();
      len_accum += node_len;
      ni = node.medial_child;
    }
  }
}

CreatedVineFoliageInstances create_leaf_branch_wind_instances(
  foliage::OrnamentalFoliageData* render_data, const VineNode* nodes, int beg, int end,
  const AxisRootInfo& axis_root_info, const RemappedAxisRoots& remapped_roots,
  const tree::Internodes& internodes, const Bounds3f& eval_aabb, float canonical_scale) {
  //
  assert(end >= beg);
  const int num_nodes = end - beg;

  CreatedVineFoliageInstances result{};
  if (num_nodes == 0) {
    return result;
  }

  Temporary<foliage::OrnamentalFoliageInstanceDescriptor, 1024> store_descs;
  auto* descs = store_descs.require(num_nodes);

  const bool is_dark = true;

  int num_descs{};
  constexpr float space = 0.75f;
  map_along_axis(nodes, beg, end, space, [&](const VineNode& node, int, float len_off) {
    if (num_descs >= num_nodes) {
      return;
    }

    PackedWindAxisRootInfo wind_axis_root_info{};
    if (node.attached_node_index >= 0 && node.attached_node_index < int(internodes.size())) {
      auto self_info = make_wind_axis_root_info(
        internodes[node.attached_node_index], internodes,
        axis_root_info, remapped_roots, eval_aabb);
      wind_axis_root_info = to_packed_wind_info(self_info, self_info);
    }

    Vec3f p = node.position + node.direction * len_off;
    descs[num_descs++] = make_vine_foliage_leaf_instance_desc(
      p, node.decode_attached_surface_normal(), 0.0f, &wind_axis_root_info, is_dark);
  });

  foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
  group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material2;
  group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::FlatPlane;
  group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnBranchAxis;
  group_desc.aggregate_aabb_p0 = eval_aabb.min;
  group_desc.aggregate_aabb_p1 = eval_aabb.max;

  const uint32_t num_process = std::min(
    uint32_t(num_descs), foliage::OrnamentalFoliageData::instance_page_size);
  result.instances = foliage::create_ornamental_foliage_instances(
    render_data, group_desc, descs, num_process);
  result.count = num_process;
  result.canonical_scale = canonical_scale;

  return result;
}

CreatedVineFoliageInstances create_leaf_instances(
  foliage::OrnamentalFoliageData* render_data, const VineNode* nodes, int beg, int end,
  float canonical_scale) {
  //
  assert(end >= beg);
  const int num_nodes = end - beg;

  CreatedVineFoliageInstances result{};
  if (num_nodes == 0) {
    return result;
  }

  Temporary<foliage::OrnamentalFoliageInstanceDescriptor, 1024> store_descs;
  auto* descs = store_descs.require(num_nodes);

  const bool is_dark = true;

  int num_descs{};
  constexpr float space = 0.75f;
  map_along_axis(nodes, beg, end, space, [&](const VineNode& node, int, float len_off) {
    if (num_descs < num_nodes) {
      Vec3f p = node.position + node.direction * len_off;
      descs[num_descs++] = make_vine_foliage_leaf_instance_desc(
        p, node.decode_attached_surface_normal(), 0.0f, nullptr, is_dark);
    }
  });

  foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
  group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material2;
  group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::FlatPlane;
  group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;

  const uint32_t num_process = std::min(
    uint32_t(num_descs), foliage::OrnamentalFoliageData::instance_page_size);
  result.instances = foliage::create_ornamental_foliage_instances(
    render_data, group_desc, descs, num_process);
  result.count = num_process;
  result.canonical_scale = canonical_scale;

  return result;
}

CreatedVineFoliageInstances create_flower_instances(
  foliage::OrnamentalFoliageData* render_data, const VineNode* nodes, int beg, int end,
  float canonical_scale, float radius_power, const CurvedPlaneMaterialParams& mat_desc) {
  //
  assert(end >= beg);
  const int num_nodes = end - beg;

  CreatedVineFoliageInstances result{};
  if (num_nodes == 0) {
    return result;
  }

  Temporary<foliage::OrnamentalFoliageInstanceDescriptor, 1024> store_descs;
  auto* descs = store_descs.require(num_nodes);

  int num_descs{};
  constexpr float space = 4.0f;

  map_along_axis(nodes, beg, end, space, [&](const VineNode& node, int, float len_off) {
    if (num_descs < num_nodes) {
      Vec3f p = node.position + node.direction * len_off;
      descs[num_descs++] = make_vine_foliage_flower_instance_desc(
        p, node.decode_attached_surface_normal(), mat_desc, radius_power, nullptr);
    }
  });

  foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
  group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material1;
  group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::CurvedPlane;
  group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;

  const uint32_t num_process = std::min(
    uint32_t(num_descs), foliage::OrnamentalFoliageData::instance_page_size);
  result.instances = foliage::create_ornamental_foliage_instances(
    render_data, group_desc, descs, num_process);
  result.count = num_process;
  result.canonical_scale = canonical_scale;

  return result;
}

[[maybe_unused]]
CreatedVineFoliageInstances create_flower_branch_wind_instances(
  foliage::OrnamentalFoliageData* render_data, const VineNode* nodes, int beg, int end,
  const AxisRootInfo& axis_root_info, const RemappedAxisRoots& remapped_roots,
  const tree::Internodes& internodes, const Bounds3f& eval_aabb, float canonical_scale,
  float radius_power, const CurvedPlaneMaterialParams& mat_desc) {
  //
  assert(end >= beg);
  const int num_nodes = end - beg;

  CreatedVineFoliageInstances result{};
  if (num_nodes == 0) {
    return result;
  }

  Temporary<foliage::OrnamentalFoliageInstanceDescriptor, 1024> store_descs;
  auto* descs = store_descs.require(num_nodes);

  int num_descs{};
  constexpr float space = 4.0f;

  map_along_axis(nodes, beg, end, space, [&](const VineNode& node, int, float len_off) {
    if (num_descs < num_nodes) {
      PackedWindAxisRootInfo wind_axis_root_info{};
      if (node.attached_node_index >= 0 && node.attached_node_index < int(internodes.size())) {
        auto self_info = make_wind_axis_root_info(
          internodes[node.attached_node_index], internodes,
          axis_root_info, remapped_roots, eval_aabb);
        wind_axis_root_info = to_packed_wind_info(self_info, self_info);
      }

      Vec3f p = node.position + node.direction * len_off;
      descs[num_descs++] = make_vine_foliage_flower_instance_desc(
        p, node.decode_attached_surface_normal(), mat_desc, radius_power, &wind_axis_root_info);
    }
  });

  foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
  group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material1;
  group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::CurvedPlane;
  group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnBranchAxis;
  group_desc.aggregate_aabb_p0 = eval_aabb.min;
  group_desc.aggregate_aabb_p1 = eval_aabb.max;

  const uint32_t num_process = std::min(
    uint32_t(num_descs), foliage::OrnamentalFoliageData::instance_page_size);
  result.instances = foliage::create_ornamental_foliage_instances(
    render_data, group_desc, descs, num_process);
  result.count = num_process;
  result.canonical_scale = canonical_scale;

  return result;
}

void set_flat_plane_scale(
  foliage::OrnamentalFoliageData* render_data, const CreatedVineFoliageInstances& insts, float scale) {
  //
  assert(scale >= 0.0f && scale <= 1.0f);
  scale *= insts.canonical_scale;
  for (uint32_t i = 0; i < insts.count; i++) {
    foliage::set_ornamental_foliage_flat_plane_scale(render_data, insts.instances, i, scale);
  }
}

void set_curved_plane_radius(
  foliage::OrnamentalFoliageData* render_data, const CreatedVineFoliageInstances& insts, float r) {
  //
  assert(r >= 0.0f && r <= 1.0f);
  r *= insts.canonical_scale;
  for (uint32_t i = 0; i < insts.count; i++) {
    foliage::set_ornamental_foliage_curved_plane_radius(render_data, insts.instances, i, r);
  }
}

void state_need_create(double curr_t, VineOrnamentalFoliageInstance& inst, const UpdateInfo& info) {
  auto seg = read_vine_segment(info.vine_sys, inst.associated_instance, inst.associated_segment);
  if (!seg.nodes || !seg.finished_growing) {
    return;
  }

  bool using_wind_instances{};
  if (seg.maybe_associated_tree_instance_id != 0) {
    TreeInstanceHandle tree_handle{seg.maybe_associated_tree_instance_id};
    if (tree_exists(info.tree_sys, tree_handle)) {
      auto tree = read_tree(info.tree_sys, tree_handle);
      if (!tree.nodes) {
        return;
      }

      auto axis_root_info = tree::compute_axis_root_info(tree.nodes->internodes);
      auto remapped_roots = tree::remap_axis_roots(tree.nodes->internodes);
      inst.leaf_instances = create_leaf_branch_wind_instances(
        info.render_data, seg.nodes, seg.node_beg, seg.node_end,
        axis_root_info, remapped_roots, tree.nodes->internodes, *tree.src_aabb, 1.0f);
#if 0
      {
        float rs;
        float rp;
        CurvedPlaneMaterialParams mat_desc;
        get_randomized_flower_params(&rs, &rp, &mat_desc);

        inst.flower_instances = create_flower_branch_wind_instances(
          info.render_data, seg.nodes, seg.node_beg, seg.node_end,
          axis_root_info, remapped_roots, tree.nodes->internodes, *tree.src_aabb,
          rs, rp, mat_desc);
      }
#endif

      using_wind_instances = true;
    }
  }

  if (!using_wind_instances) {
    inst.leaf_instances = create_leaf_instances(
      info.render_data, seg.nodes, seg.node_beg, seg.node_end, 1.0f);

    {
      float rs;
      float rp;
      CurvedPlaneMaterialParams mat_desc;
      get_randomized_flower_params(&rs, &rp, &mat_desc);

      inst.flower_instances = create_flower_instances(
        info.render_data, seg.nodes, seg.node_beg, seg.node_end, rs, rp, mat_desc);
    }
  }

  inst.state = VineOrnamentalFoliageState::Growing;
  inst.t0 = curr_t;
}

auto update_instance(
  double curr_t, VineOrnamentalFoliageInstance& inst, const UpdateInfo& info) {
  //
  struct Result {
    bool just_finished_growing;
  };

  Result result{};

  if (inst.state == VineOrnamentalFoliageState::Expired) {
    return result;
  } else if (inst.state != VineOrnamentalFoliageState::Dying &&
             !tree::vine_exists(info.vine_sys, inst.associated_instance)) {
    if (inst.state == VineOrnamentalFoliageState::NeedCreate) {
      inst.state = VineOrnamentalFoliageState::Expired;
      return result;
    } else {
      inst.state = VineOrnamentalFoliageState::Dying;
      inst.t0 = curr_t;
    }
  }

  if (inst.state == VineOrnamentalFoliageState::NeedCreate) {
    state_need_create(curr_t, inst, info);

  } else if (inst.state == VineOrnamentalFoliageState::Growing) {
    const double fade_t = 1.0;
    double t = clamp(curr_t - inst.t0, 0.0, fade_t) / fade_t;
    const auto scale = float(ease::in_out_expo(t));
    if (inst.leaf_instances.instances.is_valid()) {
      set_flat_plane_scale(info.render_data, inst.leaf_instances, scale);
    }
    if (inst.flower_instances.instances.is_valid()) {
      set_curved_plane_radius(info.render_data, inst.flower_instances, scale);
    }
    if (t == 1.0) {
      inst.state = VineOrnamentalFoliageState::Alive;
      result.just_finished_growing = true;
    }

  } else if (inst.state == VineOrnamentalFoliageState::Dying) {
    const double fade_t = 1.0;
    double t = clamp(curr_t - inst.t0, 0.0, fade_t) / fade_t;
    const auto scale = 1.0f - float(ease::in_out_expo(t));
    if (inst.leaf_instances.instances.is_valid()) {
      set_flat_plane_scale(info.render_data, inst.leaf_instances, scale);
    }
    if (inst.flower_instances.instances.is_valid()) {
      set_curved_plane_radius(info.render_data, inst.flower_instances, scale);
    }
    if (t == 1.0) {
      inst.state = VineOrnamentalFoliageState::Expired;
    }
  }

  return result;
}

void remove_expired(VineOrnamentalFoliageData* data, foliage::OrnamentalFoliageData* render_data) {
  auto it = data->instances.begin();
  while (it != data->instances.end()) {
    if (it->state == VineOrnamentalFoliageState::Expired) {
      if (it->leaf_instances.instances.is_valid()) {
        foliage::destroy_ornamental_foliage_instances(render_data, it->leaf_instances.instances);
      }
      if (it->flower_instances.instances.is_valid()) {
        foliage::destroy_ornamental_foliage_instances(render_data, it->flower_instances.instances);
      }
      it = data->instances.erase(it);
    } else {
      ++it;
    }
  }
}

struct {
  VineOrnamentalFoliageData data;
} globals;

} //  anon

void tree::create_ornamental_foliage_on_vine_segment(
  const VineInstanceHandle& inst, const VineSegmentHandle& seg) {
  //
  auto& dst_inst = globals.data.instances.emplace_back();
  dst_inst.associated_instance = inst;
  dst_inst.associated_segment = seg;
  dst_inst.state = VineOrnamentalFoliageState::NeedCreate;
}

VineOrnamentalFoliageUpdateResult
tree::update_ornamental_foliage_on_vines(const UpdateInfo& info) {
  //
  VineOrnamentalFoliageUpdateResult result{};

  const double curr_t = globals.data.stopwatch.delta().count();
  for (auto& inst : globals.data.instances) {
    auto update_res = update_instance(curr_t, inst, info);
    if (update_res.just_finished_growing) {
      result.num_finished_growing++;
    }
  }

  remove_expired(&globals.data, info.render_data);
  return result;
}

GROVE_NAMESPACE_END
