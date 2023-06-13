#include "ProceduralFlowerComponent.hpp"
#include "../model/mesh.hpp"
#include "../render/debug_draw.hpp"
#include "../procedural_tree/attraction_points.hpp"
#include "../procedural_tree/utility.hpp"
#include "../procedural_tree/bud_fate.hpp"
#include "../procedural_tree/render.hpp"
#include "../terrain/terrain.hpp"
#include "../audio_processors/Bender.hpp"
#include "../audio_observation/AudioObservation.hpp"
#include "../util/texture_io.hpp"
#include "grove/env.hpp"
#include "grove/visual/Image.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/math/random.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/load/image.hpp"
#include "grove/load/obj.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

/*
 * @TODO
 * . It's possible for a TreeNodeStore to "fail" to grow - it can be out-competed for attraction
 * points by every / any other TreeNodeStore in the vicinity. In this case the TreeNodeStore will
 * only have a single internode: the one it started out with. We may want to discard these rather
 * than proceeding to create a stem, flowers, etc. for a single node. In any case the AABB for these
 * will be empty, since we calculate the AABB that bounds the internode (base) positions, only,
 * rather than the AABB that bounds the internode cylinders; this at a minimum causes issues with
 * rendering (e.g. NaNs when evaluating a position with respect to the AABB).
 *
 */

using Stem = ProceduralFlowerComponent::Stem;
using PendingNewPlant = ProceduralFlowerComponent::PendingNewPlant;
using Flower = ProceduralFlowerComponent::Flower;
using Ornament = ProceduralFlowerComponent::Ornament;
using MakeStemParams = ProceduralFlowerComponent::MakeStemParams;
using MakePetalShapeParams = ProceduralFlowerComponent::MakePetalShapeParams;
using AlphaTestPetalMaterialParams = ProceduralFlowerComponent::AlphaTestPetalMaterialParams;

struct Config {
  static constexpr int flower_max_num_internodes = 8;
  static constexpr float root_octree_node_size = 512.0f;
  static constexpr float max_octree_span_size_split = 0.5f;
  static constexpr int flower_num_attraction_points_per_stem = 50;
  static constexpr float medial_bud_angle_criterion = 0.8f;
  static constexpr float debug_height_offset = 0.0f;
  static constexpr float port_y_offset = 4.0f;
//  static constexpr int displacement_count = 16;
  static constexpr float displace_time_s = 5.0f;
  static constexpr float pre_death_time_s = 5.0f;
  static constexpr float alive_time_s = 20.0f;
  static constexpr bool debug_attraction_points_enabled = true;
};

struct DefaultAllowStemBudSpawn {
  explicit DefaultAllowStemBudSpawn(int max_num_spawn) : max_num_lateral_spawn{max_num_spawn} {
    //
  }

  bool operator()(const tree::Internodes& inodes, const tree::Bud& bud, const Vec3f& shoot_dir);

  int num_lateral_spawned{};
  int max_num_lateral_spawn;
};

inline bool DefaultAllowStemBudSpawn::operator()(const tree::Internodes& inodes,
                                                 const tree::Bud& bud,
                                                 const Vec3f& shoot_dir) {
  if (bud.is_terminal) {
    auto& prev_dir = inodes[bud.parent].direction;
    return dot(prev_dir, shoot_dir) >= Config::medial_bud_angle_criterion;
  }
  if (num_lateral_spawned < max_num_lateral_spawn &&
      inodes[bud.parent].gravelius_order == 0) {
    num_lateral_spawned++;
    return true;
  } else {
    return false;
  }
}

Stem make_stem(const MakeStemParams& params) {
  Stem result{};
  result.spawn_params = tree::SpawnInternodeParams::make_debug(params.scale);
  result.spawn_params.max_num_internodes = params.max_num_internodes;
  result.bud_q_params = tree::DistributeBudQParams::make_debug();
  result.spawn_params.allow_spawn_func = DefaultAllowStemBudSpawn{params.max_num_lateral_axes};
  result.spawn_params.leaf_diameter *= 2.0f;
  result.max_num_internodes = params.max_num_internodes;
  result.color = params.color;

  const auto& sp = result.spawn_params;
  const auto inode_scale = sp.internode_length_scale;
  const auto bud_pa = sp.bud_perception_angle;
  const auto bud_pd = sp.bud_perception_distance;
  const auto bud_ozr = sp.bud_occupancy_zone_radius;

  result.nodes = tree::make_tree_node_store(params.origin, inode_scale, bud_pa, bud_pd, bud_ozr);
  return result;
}

tree::AttractionPoints make_octree() {
  return tree::AttractionPoints{Config::root_octree_node_size, Config::max_octree_span_size_split};
}

std::vector<int> growable_stems(const std::vector<Stem>& stems) {
  std::vector<int> result;
  for (int i = 0; i < int(stems.size()); i++) {
    if (stems[i].can_grow) {
      result.push_back(i);
    }
  }
  return result;
}

[[maybe_unused]] std::vector<Vec3f> low_to_ground_attraction_points(int n,
                                                                    const Vec3f& scale,
                                                                    const Vec3f& ori) {
//  auto scl = Vec3f{4.0f, 1.0f, 4.0f} * scale;
  auto scl = scale;
  return points::uniform_hemisphere(n, scl, ori);
}

[[maybe_unused]] std::vector<Vec3f> high_above_ground_attraction_points(int n,
                                                                        const Vec3f& scale,
                                                                        const Vec3f& ori) {
  auto scl = Vec3f{2.0f, 4.0f, 2.0f} * scale;
  return points::uniform_cylinder_to_hemisphere(n, scl, ori);
}

std::vector<Vec3f> flower_attraction_points(int n, const Vec3f& scale, const Vec3f& off) {
  auto ps = points::uniform_hemisphere(n);
  for (auto& p : ps) {
    assert(p.y >= 0.0f);
    const float xz_scale = 0.05f + std::max(0.0f, p.y - 0.25f) / 0.75f;
    p.x *= xz_scale;
    p.z *= xz_scale;
    p *= scale;
    p += off;
  }
  return ps;
}

bool finished_growing(const Stem& stem) {
  const auto num_inodes = int(stem.nodes.internodes.size());
  return stem.last_num_internodes == num_inodes || num_inodes >= stem.max_num_internodes;
}

bool finished_growing(const Ornament& orn) {
  return orn.growth_frac >= 1.0f;
}

bool finished_growing(const Flower& flower) {
  auto& ornaments = flower.ornaments;
  return std::all_of(ornaments.begin(), ornaments.end(), [](const Ornament& orn) {
    return finished_growing(orn);
  });
}

std::vector<const tree::Internode*> gather_leaves(const tree::Internodes& internodes) {
  std::vector<const tree::Internode*> result;
  for (auto& node : internodes) {
    if (node.is_leaf()) {
      result.push_back(&node);
    }
  }
  return result;
}

std::vector<const tree::Internode*> gather_medial_grav_order0(const tree::Internodes& internodes) {
  std::vector<const tree::Internode*> result;
  tree::map_axis([&internodes, &result](tree::TreeNodeIndex ind) {
    const auto* node = &internodes[ind];
    if (node->gravelius_order == 0 &&
        node->has_parent() &&
        !node->has_lateral_child()) {
      result.push_back(node);
    }
  }, internodes);
  return result;
}

tree::RenderAxisGrowthContext initialize_axis_render_growth(tree::Internodes& inodes) {
  tree::RenderAxisGrowthContext context;
  const tree::TreeNodeIndex root_index = 0;
  tree::initialize_axis_render_growth_context(&context, inodes, root_index);
  tree::set_render_length_scale(inodes, root_index, 0.0f);
  return context;
}

Flower make_flower(PendingNewPlant&& pend) {
  Flower flower{};
  flower.ornaments = std::move(pend.ornaments);
  return flower;
}

#if 0
int rand_proc_petal_type() {
  return int(urand() * 3.0);
}
#endif

MakeStemParams make_flower_make_stem_params(const Vec2f& stem_ori, float stem_scale) {
  MakeStemParams result{};
  result.color = Vec3f{192.0f/255.0f, 251.0f/255.0f, 166.0f/255.0f};
  result.origin = Vec3f{stem_ori.x, 0.0f, stem_ori.y};
  result.scale = stem_scale;
  result.max_num_lateral_axes = 4;
  result.max_num_internodes = Config::flower_max_num_internodes;
  result.make_attraction_points = [](const Vec3f& ori, float scale) {
    return flower_attraction_points(
      Config::flower_num_attraction_points_per_stem, Vec3f{scale}, ori);
  };
  return result;
}

AlphaTestPetalMaterialParams make_debug_alpha_test_petal_material_params(int num_texture_layers) {
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
  AlphaTestPetalMaterialParams result{};
  result.color0 = color0;
  result.color1 = color1;
  result.color2 = color2;
  result.color3 = color3;
  result.texture_layer = int(urandf() * float(num_texture_layers));
  return result;
}

MakePetalShapeParams make_petal_shape_params_func(float radius_power, float radius_scale) {
  return [radius_power, radius_scale](float gf, float df) {
    auto shape = petal::ShapeParams::plane(gf, df, radius_scale, radius_power);
    shape.curl_scale = (1.0f - std::pow(gf, 2.0f)) * radius_scale * 2.0f;
    shape.min_radius = 0.01f;
    return shape;
  };
}

PendingNewPlant make_alpha_test_procedural_pending_plant(MakeStemParams&& stem_params,
                                                         const AlphaTestPetalMaterialParams& mat_params,
                                                         int num_ornaments,
                                                         float radius_power, float radius_scale) {
  PendingNewPlant pend{};
  pend.make_stem_params = std::move(stem_params);

  for (int i = 0; i < num_ornaments; i++) {
    Ornament ornament{};
    float r_scale;
    if (i == 0) {
      r_scale = 1.0f;
    } else if (i == 1) {
      r_scale = 0.25f;
    } else {
      r_scale = 0.5f;
    }
    ornament.shape = make_petal_shape_params_func(radius_power, r_scale * radius_scale);
    ornament.alpha_test_petal_material_params = mat_params;
    ornament.scale = 0.5f;
    ornament.growth_incr_randomness = urand_11f() * 0.75f;
    pend.ornaments.push_back(std::move(ornament));
  }
  return pend;
}

using CreateNodeRes = SimpleAudioNodePlacement::CreateNodeResult;
CreateNodeRes make_bender_instrument(const ProceduralFlowerComponent::InitInfo& init_info,
                                     ProceduralFlowerBenderInstrument& bender_instrument,
                                     const Vec3f& position, AudioNodeStorage::NodeID* dst_node_id) {
  auto& node_storage = init_info.node_storage;
  const auto* transport = init_info.transport;
  auto& observation = init_info.audio_observation;
  auto& placement = init_info.node_placement;

  auto node_ctor = [transport](AudioNodeStorage::NodeID node_id) {
    bool emit_events = true;
    return new Bender(node_id, transport, emit_events);
  };

  auto node = node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
  auto monitorable_node = bender_instrument.create_instance(node);
  observation.parameter_monitor.add_node(node, std::move(monitorable_node));
  *dst_node_id = node;

  auto port_info = node_storage.get_port_info_for_node(node).unwrap();
  auto orientation = SimpleAudioNodePlacement::NodeOrientation::Horizontal;
  return placement.create_node(node, port_info, position, Config::port_y_offset, orientation);
}

Optional<Vec3f> tallest_leaf_position(const tree::Internodes& nodes) {
  float mx{-infinityf()};
  int max_ind{-1};
  for (int i = 0; i < int(nodes.size()); i++) {
    if (nodes[i].is_leaf()) {
      auto tip_pos = nodes[i].render_tip_position();
      if (tip_pos.y > mx) {
        mx = tip_pos.y;
        max_ind = i;
      }
    }
  }
  return max_ind < 0 ? NullOpt{} : Optional<Vec3f>(nodes[max_ind].render_tip_position());
}

[[maybe_unused]] Optional<Image<uint8_t>> load_image_in_resource_dir(const char* p) {
  auto im_p = std::string{GROVE_ASSET_DIR} + p;
  bool success{};
  auto im = load_image(im_p.c_str(), &success, true);
  if (success) {
    return Optional<Image<uint8_t>>(std::move(im));
  } else {
    return NullOpt{};
  }
}

AlphaTestPetalMaterialParams permute(AlphaTestPetalMaterialParams p, const Vec3<int>& pi) {
  p.color0 = permute(p.color0, pi.x, pi.y, pi.z);
  p.color1 = permute(p.color1, pi.x, pi.y, pi.z);
  p.color2 = permute(p.color2, pi.x, pi.y, pi.z);
  p.color3 = permute(p.color3, pi.x, pi.y, pi.z);
  return p;
}

#if 0
[[maybe_unused]] Optional<Image<uint8_t>> load_debug_texture_image() {
//  return load_image_in_resource_dir("/models/petal1/leaf1.png");
  return load_image_in_resource_dir("/models/petal1/leaves-path.png");
}

[[maybe_unused]] Optional<Image<uint8_t>> load_debug_plane_texture_image() {
//  return load_image_in_resource_dir("/models/petal1/petal1-style-512.png");
  return load_image_in_resource_dir("/models/petal1/lilly-redux.png");
}

[[maybe_unused]] Optional<Image<uint8_t>> load_debug_daffodil_texture() {
  return load_image_in_resource_dir("/models/petal1/daffodil3.png");
}
#endif

[[maybe_unused]] Optional<obj::VertexData> load_debug_petal_model() {
  auto model_dir = std::string{GROVE_ASSET_DIR} + "/models/petal1";
//  auto model_p = model_dir + "/petal-template2-tp.obj";
  auto model_p = model_dir + "/leaf1.obj";
  bool success{};
  auto load_res = obj::load_simple(model_p.c_str(), model_dir.c_str(), &success);
  if (success) {
    return Optional<obj::VertexData>(std::move(load_res));
  } else {
    return NullOpt{};
  }
}

[[maybe_unused]] bool find_position_uv_attrs(const obj::VertexData& data, int* pos, int* uv) {
  if (auto pos_ind = data.find_attribute(obj::AttributeType::Position)) {
    *pos = pos_ind.value();
  } else {
    return false;
  }
  if (auto uv_ind = data.find_attribute(obj::AttributeType::TexCoord)) {
    *uv = uv_ind.value();
  } else {
    return false;
  }
  return true;
}

void set_enable_randomization(ProceduralFlowerComponent::Params& params) {
  params.randomize_flower_radius_scale = true;
  params.randomize_flower_radius_power = true;
  params.flower_radius_power_randomness = 0.33f;
  params.flower_radius_randomness = 0.33f;
}

} //  anon

using InitResult = ProceduralFlowerComponent::InitResult;
InitResult ProceduralFlowerComponent::initialize(const InitInfo& init_info) {
  InitResult result;

  ornament_particles.initialize();
  attraction_points = make_octree();

  params.axis_growth_incr = 0.1f;
  params.ornament_growth_incr = 0.1f;

  {
    Vec3f bender_ori{0.0f, Config::port_y_offset, 0.0f};
    bender_ori.y += init_info.terrain.height_nearest_position_xz(bender_ori);

    AudioNodeStorage::NodeID node_id{};
    result.pending_placement = make_bender_instrument(
      init_info, bender_instrument, bender_ori, &node_id);

    result.insert_audio_node_bounds_into_accel = init_info.node_placement.get_node_bounds(
      node_id, init_info.node_storage, init_info.terrain);
  }

  if (init_info.octree_point_drawable) {
    debug_attraction_points_drawable = init_info.octree_point_drawable.value();
  }

  num_alpha_test_texture_layers = init_info.num_material1_alpha_test_texture_layers;

  params.allow_bush = false;
  set_enable_randomization(params);
  params.disable_alpha_test_ornaments = true;

  return result;
}

bool ProceduralFlowerComponent::should_start_growing() const {
  //  Grow if not growing and any stem can still grow.
  if (!growing && stem_growth_cycle_context.state == tree::GrowthState::Idle) {
    for (auto& stem : stems) {
      if (stem.can_grow) {
        return true;
      }
    }
  }
  return false;
}

bool ProceduralFlowerComponent::should_stop_growing() const {
  return growing && stem_growth_cycle_context.state == tree::GrowthState::Idle;
}

void ProceduralFlowerComponent::on_growth_cycle_start(const UpdateInfo&) {
  auto growable_inds = growable_stems(stems);

  std::vector<const tree::DistributeBudQParams*> distrib_params(growable_inds.size());
  std::vector<const tree::SpawnInternodeParams*> spawn_params(growable_inds.size());
  std::vector<tree::TreeNodeStore*> stem_ptrs(growable_inds.size());

  for (int i = 0; i < int(growable_inds.size()); i++) {
    int ind = growable_inds[i];
    distrib_params[i] = &stems[ind].bud_q_params;
    spawn_params[i] = &stems[ind].spawn_params;
    stem_ptrs[i] = &stems[ind].nodes;
  }

  tree::initialize_growth_cycle(stem_growth_cycle_context,
                                &attraction_points,
                                std::move(stem_ptrs),
                                std::move(spawn_params),
                                std::move(distrib_params));
}

void ProceduralFlowerComponent::maybe_make_plants(const UpdateInfo& update_info) {
  while (!pending_new_plants.empty()) {
    auto pend = std::move(pending_new_plants.back());
    pending_new_plants.pop_back();

    auto& stem_ori = pend.make_stem_params.origin;
    stem_ori.y = update_info.terrain.height_nearest_position_xz(stem_ori) + Config::debug_height_offset;
    stems.push_back(make_stem(pend.make_stem_params));
    auto stem_id = stems.back().nodes.id;

    auto ps = pend.make_stem_params.make_attraction_points(stem_ori, pend.make_stem_params.scale);
    for (auto& p : ps) {
      attraction_points.insert(p, tree::make_attraction_point(p, stem_id.id));
    }

    flowers[stem_id] = make_flower(std::move(pend));

    params.need_update_debug_octree = true;
  }
}

void ProceduralFlowerComponent::update_growth(const UpdateInfo& update_info) {
  if (!growing) {
    maybe_make_plants(update_info);
  }

  if (should_start_growing()) {
    growing = true;
    on_growth_cycle_start(update_info);

  } else if (should_stop_growing()) {
    growing = false;
    on_growth_cycle_end(update_info);
  }

  tree::GrowthCycleParams growth_cycle_params{};
  growth_cycle_params.time_limit_seconds = params.growth_time_limit_seconds;
  tree::growth_cycle(stem_growth_cycle_context, growth_cycle_params);
}

void ProceduralFlowerComponent::on_growth_cycle_end(const UpdateInfo& info) {
  for (auto& stem : stems) {
    if (stem.can_grow) {
      if (finished_growing(stem)) {
        //  @TODO: See above regarding deleting stem with nodes <= 1
        stem.can_grow = false;
        stem.finished_growing = true;

        tree::set_render_length_scale(stem.nodes.internodes, 0, 0.0f);
        if (!stem.drawable) {
          ProceduralFlowerStemRenderer::DrawableParams stem_draw_params{};
          stem_draw_params.color = stem.color;
          stem.drawable = info.stem_renderer.create_drawable(
            info.stem_context, stem.nodes.internodes, stem_draw_params);
        }
        //  Start appearing to grow.
        stem_render_growth_contexts[stem.nodes.id] =
          initialize_axis_render_growth(stem.nodes.internodes);
      }
      stem.last_num_internodes = int(stem.nodes.internodes.size());
    }
  }
}

void ProceduralFlowerComponent::apply_growth_death_fraction(Ornament& orn, const UpdateInfo& info) {
  if (orn.foliage_instance_handle) {
    auto shape = orn.shape(orn.growth_frac, orn.death_frac);
    foliage::CurvedPlaneGeometryDescriptor geom_desc{};
    geom_desc.min_radius = shape.min_radius;
    geom_desc.radius_power = shape.radius_power;
    geom_desc.radius = shape.radius;
    geom_desc.curl_scale = shape.curl_scale;
    foliage::set_ornamental_foliage_curved_plane_geometry(
      info.ornamental_foliage_data, orn.foliage_instance_handle.value(), geom_desc);
  }
}

void ProceduralFlowerComponent::add_procedural_ornament(const UpdateInfo& info,
                                                        Ornament& orn,
                                                        const std::vector<const tree::Internode*>& internodes,
                                                        const Bounds3f& node_aabb) {
  if (num_alpha_test_texture_layers == 0) {
    return;
  }

  auto shape_params = orn.shape(0.0f, 0.0f);
  const AlphaTestPetalMaterialParams& mat_params = orn.alpha_test_petal_material_params;
  (void) node_aabb;

  const Vec3f world_ori = node_aabb.center();
  const Vec2f ori_xz = Vec2f{world_ori.x, world_ori.z};
  const bool is_empty = node_aabb.size() == Vec3f{};

  {
    assert(!orn.foliage_instance_handle);
    Temporary<foliage::OrnamentalFoliageInstanceDescriptor, 128> store_instance_descs;
    auto* instance_descs = store_instance_descs.require(int(internodes.size()));
    for (int i = 0; i < int(internodes.size()); i++) {
      auto& node = internodes[i];
      const float tip_y_frac = is_empty ? 0.0f : clamp01(node_aabb.to_fraction(node->position).y);
      foliage::OrnamentalFoliageInstanceDescriptor desc{};
      desc.translation = node->render_tip_position() + node->direction * orn.tip_offset;
      desc.orientation = node->direction;
      desc.material.material1.texture_layer_index = mat_params.texture_layer;
      desc.material.material1.color0 = mat_params.color0;
      desc.material.material1.color1 = mat_params.color1;
      desc.material.material1.color2 = mat_params.color2;
      desc.material.material1.color3 = mat_params.color3;
      desc.geometry_descriptor.curved_plane.min_radius = shape_params.min_radius;
      desc.geometry_descriptor.curved_plane.radius = shape_params.radius;
      desc.geometry_descriptor.curved_plane.radius_power = shape_params.radius_power;
      desc.geometry_descriptor.curved_plane.curl_scale = shape_params.curl_scale;
      desc.wind_data.on_plant_stem.tip_y_fraction = tip_y_frac;
      desc.wind_data.on_plant_stem.world_origin_xz = ori_xz;
      instance_descs[i] = desc;
    }
    foliage::OrnamentalFoliageInstanceGroupDescriptor group_desc{};
    group_desc.material_type = foliage::OrnamentalFoliageMaterialType::Material1;
    group_desc.wind_type = foliage::OrnamentalFoliageWindType::OnPlantStem;
    group_desc.geometry_type = foliage::OrnamentalFoliageGeometryType::CurvedPlane;
    const uint32_t num_inst_add = std::min(
      uint32_t(internodes.size()), foliage::OrnamentalFoliageData::instance_page_size);
    orn.foliage_instance_handle = foliage::create_ornamental_foliage_instances(
      info.ornamental_foliage_data, group_desc, instance_descs, num_inst_add);
  }
}

void ProceduralFlowerComponent::update_stem_axis_growth(const UpdateInfo& update_info) {
  auto stem_it = stem_render_growth_contexts.begin();
  while (stem_it != stem_render_growth_contexts.end()) {
    auto& id = stem_it->first;
    auto& context = stem_it->second;
    auto* stem = find_stem_by_id(id);
    if (!stem) {
      assert(false);
      stem_it = stem_render_growth_contexts.erase(stem_it);
      continue;
    }

    bool do_erase = true;
    bool still_growing = tree::update_render_growth(
      stem->nodes.internodes, stem->spawn_params, context, params.axis_growth_incr);
    if (still_growing) {
      //  Still growing.
      do_erase = false;
      if (auto handle = stem->drawable) {
        update_info.stem_renderer.set_dynamic_data(handle.value(), stem->nodes.internodes);
      }
    } else {
      //  Finished growing.
      const auto internode_aabb = tree::internode_aabb(stem->nodes.internodes);
      if (auto* flower = find_flower(id)) {
        //  Create ornaments and start ornament growth.
        flower->ornaments_can_grow = true;
        const auto leaves = gather_leaves(stem->nodes.internodes);
        const auto medial = gather_medial_grav_order0(stem->nodes.internodes);
        for (auto& orn : flower->ornaments) {
          add_procedural_ornament(update_info, orn, leaves, internode_aabb);
        }
      }
    }
    if (do_erase) {
      stem_it = stem_render_growth_contexts.erase(stem_it);
    } else {
      ++stem_it;
    }
  }
}

int ProceduralFlowerComponent::update_ornament_growth(const UpdateInfo& update_info) {
  int num_finished{};
  for (auto& [id, flower] : flowers) {
    if (!flower.ornaments_can_grow || flower.finished_render_growing) {
      continue;
    }
    bool maybe_finished_render_growing{true};
    for (auto& orn : flower.ornaments) {
      if (orn.growth_frac < 1.0f) {
        float growth_incr = std::max(0.001f, params.ornament_growth_incr +
          (params.ornament_growth_incr * orn.growth_incr_randomness));

        maybe_finished_render_growing = false;
        orn.growth_frac = std::min(1.0f, orn.growth_frac + growth_incr);
        apply_growth_death_fraction(orn, update_info);
      }
    }
    if (maybe_finished_render_growing) {
      flower.finished_render_growing = true;
      flower.state_timer.reset();
      num_finished++;
    }
  }
  return num_finished;
}

void ProceduralFlowerComponent::update_ornament_death(const UpdateInfo& info) {
  for (auto& [id, flower] : flowers) {
    if (params.death_enabled &&
        flower.finished_render_growing &&
        !flower.finished_ornament_dying) {
      auto alive_t = flower.state_timer.delta().count();
      if (alive_t > Config::alive_time_s) {
        bool maybe_finished_dying{true};
        for (auto& orn : flower.ornaments) {
          if (orn.death_frac < 1.0f) {
            maybe_finished_dying = false;
            orn.death_frac = std::min(1.0f, orn.death_frac + params.ornament_growth_incr);
            apply_growth_death_fraction(orn, info);
          }
        }
        if (maybe_finished_dying) {
          flower.finished_ornament_dying = true;
          flower.state_timer.reset();
        }
      }
    }
  }
}

void ProceduralFlowerComponent::update_ornament_dispersal(const UpdateInfo&) {
  for (auto& [id, flower] : flowers) {
    if (!flower.finished_ornament_dying || flower.finished_ornament_dispersal) {
      continue;
    }
    auto* stem = find_stem_by_id(id);
    if (!stem) {
      assert(false);
      continue;
    }
    auto dying_t = float(flower.state_timer.delta().count());
    const float pre_dying_t = Config::pre_death_time_s;
    if (dying_t < pre_dying_t) {
      continue;
    }

    const float frac_finished = std::min(1.0f, (dying_t - pre_dying_t) / Config::displace_time_s);
#if 0
    for (auto& orn : flower.ornaments) {
      const int displace_count = orn.displacement_count;
      if (orn.displacement_handle.is_valid()) {
        if (!orn.particle_handles) {
          //  Spawn particles.
          ProceduralFlowerOrnamentParticles::SpawnParams spawn_params{};
          spawn_params.origin = stem_ori;
          orn.particle_handles = std::make_unique<PartHandle[]>(displace_count);
          orn.particle_displacement = std::make_unique<Vec4f[]>(displace_count);
          for (int i = 0; i < displace_count; i++) {
            orn.particle_handles[i] = ornament_particles.spawn_particle(spawn_params);
          }
        }
        for (int i = 0; i < displace_count; i++) {
          orn.particle_displacement[i] = Vec4f{
            ornament_particles.get_displacement(orn.particle_handles[i]),
            frac_finished
          };
        }
        petal_renderer.set_displacement(
          orn.displacement_handle,
          orn.particle_displacement.get(),
          displace_count);
      }
    }
#endif
    if (frac_finished >= 1.0f) {
      flower.finished_ornament_dispersal = true;
#if 0
      for (auto& orn : flower.ornaments) {
        if (orn.particle_handles) {
          for (int i = 0; i < orn.displacement_count; i++) {
            ornament_particles.remove_particle(orn.particle_handles[i]);
          }
          orn.particle_handles = nullptr;
          orn.particle_displacement = nullptr;
        }
      }
#endif
      //  Start axis death
      stem_render_death_contexts[id] =
        tree::make_default_render_axis_death_context(stem->nodes.internodes);
    }
  }
}

void ProceduralFlowerComponent::update_stem_axis_death(const UpdateInfo& info) {
  auto stem_it = stem_render_death_contexts.begin();
  while (stem_it != stem_render_death_contexts.end()) {
    auto& id = stem_it->first;
    auto& context = stem_it->second;
    auto* stem = find_stem_by_id(id);
    if (!stem) {
      assert(false);
      stem_it = stem_render_death_contexts.erase(stem_it);
      continue;
    }

    bool do_erase = true;
    bool still_dying = tree::update_render_death(
      stem->nodes.internodes, stem->spawn_params, context, params.axis_growth_incr);
    if (still_dying) {
      do_erase = false;
      if (auto handle = stem->drawable) {
        info.stem_renderer.set_dynamic_data(handle.value(), stem->nodes.internodes);
      }
    } else {
      //  Finished dying
    }
    if (do_erase) {
      stem_it = stem_render_death_contexts.erase(stem_it);
    } else {
      ++stem_it;
    }
  }
}

void ProceduralFlowerComponent::update_bender_instrument(const UpdateInfo&,
                                                         UpdateResult& out) {
  auto instr_update_res = bender_instrument.update();
  if (instr_update_res.spawn_particle) {
    if (auto* stem = uniform_array_sample(stems.data(), stems.size())) {
      bool can_spawn = false;
      if (auto* flower = find_flower(stem->nodes.id)) {
        can_spawn = finished_growing(*flower);
#if 1
        auto ci = petal::MaterialParams::component_indices_from_perm_index(
          petal::MaterialParams::random_perm_index());
        for (auto& orn : flower->ornaments) {
          orn.alpha_test_petal_material_params = permute(orn.alpha_test_petal_material_params, ci);
        }
#endif
      }
      if (auto ori = tallest_leaf_position(stem->nodes.internodes); ori && can_spawn) {
        SpawnPollenParticle to_spawn{};
        to_spawn.position = ori.value();
        out.spawn_pollen_particles.push_back(to_spawn);
      }
    }
  }
}

void ProceduralFlowerComponent::update_ornament_particles(const UpdateInfo& update_info,
                                                          UpdateResult&) {
  ornament_particles.update({
    update_info.wind,
    update_info.real_dt,
    params.ornament_particles_dt_scale
  });
}

void ProceduralFlowerComponent::update_debug_attraction_points_drawable(const UpdateInfo&,
                                                                        UpdateResult& out) {
  if (params.toggle_render_attraction_points) {
    params.render_attraction_points = params.toggle_render_attraction_points.value();
    if (debug_attraction_points_drawable) {
      out.toggle_debug_attraction_points_drawable = debug_attraction_points_drawable.value();
    }
    params.toggle_render_attraction_points = NullOpt{};
  }
  if (params.need_update_debug_octree &&
      params.render_attraction_points &&
      debug_attraction_points_drawable) {
    UpdatePointBuffer update_point_buffer;
    update_point_buffer.handle = debug_attraction_points_drawable.value();
    update_point_buffer.points = tree::extract_octree_points(attraction_points);
    params.need_update_debug_octree = false;
    out.update_debug_attraction_points = std::move(update_point_buffer);
  }
}

using UpdateResult = ProceduralFlowerComponent::UpdateResult;
UpdateResult ProceduralFlowerComponent::update(const UpdateInfo& update_info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("ProceduralFlowerComponent/update");
  (void) profiler;

  UpdateResult result;

  if (params.need_add_patch_at_cursor) {
    Vec2f pos{update_info.cursor_tform_position.x, update_info.cursor_tform_position.z};
    add_patch(pos);
    params.need_add_patch_at_cursor = false;
  }

  update_ornament_particles(update_info, result);
  update_bender_instrument(update_info, result);
  update_growth(update_info);

  if (!growing) {
    update_stem_axis_growth(update_info);
    result.num_ornaments_finished_growing = update_ornament_growth(update_info);
    update_ornament_death(update_info);
    update_ornament_dispersal(update_info);
    update_stem_axis_death(update_info);
    if constexpr (Config::debug_attraction_points_enabled) {
      update_debug_attraction_points_drawable(update_info, result);
    }
  }

  if (selected_flower) {
    if (auto* stem = find_stem_by_id(selected_flower.value())) {
      auto draw_at = stem->nodes.origin() + Vec3f{0.0f, 4.0f, 0.0f};
      vk::debug::draw_cube(draw_at, Vec3f{0.5f}, Vec3f{1.0f});
    }
  }

  return result;
}

void ProceduralFlowerComponent::add_patch_at_cursor_position() {
  params.need_add_patch_at_cursor = true;
}

void ProceduralFlowerComponent::add_patch(const Vec2f& pos_xz) {
  auto mat_params = make_debug_alpha_test_petal_material_params(num_alpha_test_texture_layers);
  const int num_ornaments = urandf() > 0.5f ? 3 : 1;
//  const int num_ornaments = 2;

  float rp = params.flower_radius_power;
  if (params.randomize_flower_radius_power) {
    const float rps[3] = {0.5f, 2.0f, 5.0f};
    rp = *uniform_array_sample(rps, 3);
  }

  float rs = params.flower_radius_scale;
  if (params.randomize_flower_radius_scale) {
    const float rss[4] = {1.0f, 0.5f, 0.75f, 1.5f};
    rs = *uniform_array_sample(rss, 4);
  }

  auto global_off = pos_xz + params.patch_position_radius * Vec2f{urand_11f(), urand_11f()};

  for (int i = 0; i < params.patch_size; i++) {
    auto patch_off = Vec2f{urand_11f(), urand_11f()} * params.patch_radius * 0.5f;
    auto make_stem_params = make_flower_make_stem_params(
      global_off + patch_off, params.flower_stem_scale);

    auto pend = make_alpha_test_procedural_pending_plant(
      std::move(make_stem_params),
      mat_params,
      num_ornaments,
      rp + (rp * urand_11f() * params.flower_radius_power_randomness),
      rs + (rs * urand_11f() * params.flower_radius_randomness));

    pending_new_plants.push_back(std::move(pend));
  }
}

Stem* ProceduralFlowerComponent::find_stem_by_id(tree::TreeID id) {
  for (auto& s : stems) {
    if (s.nodes.id == id) {
      return &s;
    }
  }
  return nullptr;
}

Flower* ProceduralFlowerComponent::find_flower(tree::TreeID id) {
  auto it = flowers.find(id);
  return it == flowers.end() ? nullptr : &it->second;
}

const Flower* ProceduralFlowerComponent::find_flower(tree::TreeID id) const {
  auto it = flowers.find(id);
  return it == flowers.end() ? nullptr : &it->second;
}

void ProceduralFlowerComponent::on_gui_update(const ProceduralFlowerGUI::UpdateResult& update_res) {
  if (update_res.render_attraction_points) {
    params.toggle_render_attraction_points = update_res.render_attraction_points.value();
  }
  if (update_res.death_enabled) {
    params.death_enabled = update_res.death_enabled.value();
  }
  if (update_res.add_patch) {
    params.need_add_patch_at_cursor = true;
  }
  if (update_res.patch_size) {
    params.patch_size = update_res.patch_size.value();
  }
  if (update_res.patch_radius) {
    params.patch_radius = update_res.patch_radius.value();
  }
  if (update_res.patch_position_radius) {
    params.patch_position_radius = update_res.patch_position_radius.value();
  }
  if (update_res.flower_stem_scale) {
    params.flower_stem_scale = update_res.flower_stem_scale.value();
  }
  if (update_res.flower_radius_power) {
    params.flower_radius_power = update_res.flower_radius_power.value();
  }
  if (update_res.flower_radius_scale) {
    params.flower_radius_scale = update_res.flower_radius_scale.value();
  }
  if (update_res.flower_radius_randomness) {
    params.flower_radius_randomness = update_res.flower_radius_randomness.value();
  }
  if (update_res.flower_radius_power_randomness) {
    params.flower_radius_power_randomness = update_res.flower_radius_power_randomness.value();
  }
  if (update_res.randomize_flower_radius_power) {
    params.randomize_flower_radius_power = update_res.randomize_flower_radius_power.value();
  }
  if (update_res.randomize_flower_radius_scale) {
    params.randomize_flower_radius_scale = update_res.randomize_flower_radius_scale.value();
  }
  if (update_res.ornament_growth_incr) {
    params.ornament_growth_incr = update_res.ornament_growth_incr.value();
  }
  if (update_res.axis_growth_incr) {
    params.axis_growth_incr = update_res.axis_growth_incr.value();
  }
  if (update_res.enable_randomization) {
    set_enable_randomization(params);
  }
  if (update_res.selected_flower) {
    auto id = tree::TreeID{update_res.selected_flower.value()};
    if (flowers.count(id) > 0) {
      selected_flower = id;
    }
  }
  if (update_res.allow_bush) {
    params.allow_bush = update_res.allow_bush.value();
  }
}

#if 0
bool ProceduralFlowerComponent::render_gui(IMGUIWrapper& imgui_wrapper) {
  auto update_res = gui.render_gui(imgui_wrapper, *this);
  if (update_res.render_attraction_points) {
    params.render_debug_octree = update_res.render_attraction_points.value();
  }
  if (update_res.render_petals) {
    params.render_petals = update_res.render_petals.value();
  }
  if (update_res.render_stems) {
    params.render_stems = update_res.render_stems.value();
  }
  if (update_res.reload_petal_program) {
    params.need_reload_petal_program = true;
  }
  if (update_res.alpha_test_enabled) {
    params.alpha_test_enabled = update_res.alpha_test_enabled.value();
  }
  if (update_res.death_enabled) {
    params.death_enabled = update_res.death_enabled.value();
  }
  if (update_res.ornament_growth_incr) {
    params.ornament_growth_incr = update_res.ornament_growth_incr.value();
  }
  if (update_res.set_shape_params) {
    auto& set_shape_params = update_res.set_shape_params.value();
    if (auto it = flowers.find(set_shape_params.tree_id); it != flowers.end()) {
      for (auto& orn : it->second.ornaments) {
        if (orn.proc_geom_handle.is_valid() &&
            orn.proc_geom_handle == set_shape_params.geom_handle) {
          petal_renderer.set_shape_params(
            orn.proc_geom_handle, set_shape_params.shape_params);
          break;
        }
      }
    }
  }
  if (update_res.set_growth_frac) {
    auto& set_growth_frac = update_res.set_growth_frac.value();
    if (auto it = flowers.find(set_growth_frac.tree_id); it != flowers.end()) {
      for (auto& orn : it->second.ornaments) {
        if (orn.proc_geom_handle.is_valid()) {
          petal_renderer.set_shape_params(
            orn.proc_geom_handle, orn.shape(set_growth_frac.growth_frac,
                                            set_growth_frac.death_frac));
        } else if (orn.static_instance_handle.is_valid()) {
          ProceduralFlowerPetalRenderer::StaticInstanceParams inst_params{};
          inst_params.scale = orn.static_params.lerp_scale(set_growth_frac.growth_frac);
          petal_renderer.set_static_instance_params(orn.static_instance_handle, inst_params);
        }
      }
    }
  }
  return update_res.close_window;
}
#endif

GROVE_NAMESPACE_END
