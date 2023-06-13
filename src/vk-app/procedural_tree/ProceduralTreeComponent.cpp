#include "ProceduralTreeComponent.hpp"
#include "vine_ornamental_foliage.hpp"
#include "attraction_points.hpp"
#include "bud_fate.hpp"
#include "utility.hpp"
#include "serialize.hpp"
#include "../terrain/terrain.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "../audio_observation/AudioObservation.hpp"
#include "debug_growth_system.hpp"
#include "resource_flow_along_nodes.hpp"
#include "../environment/season.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/math/util.hpp"
#include "grove/math/random.hpp"
#include "grove/math/ease.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"
#include "grove/visual/Camera.hpp"

#define INCLUDE_SOIL (1)
#if INCLUDE_SOIL
#include "../terrain/Soil.hpp"
#endif

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr int max_num_internodes = 512;
  static constexpr int remove_if_fewer_than_n_internodes_after_pruning = 2;
  static constexpr int remove_if_fewer_than_n_internodes_after_growing = 16;
#ifdef GROVE_DEBUG
//  static constexpr int initial_num_trees = 5;
  static constexpr int initial_num_trees = 1;
#else
  static constexpr int initial_num_trees = 1;
#endif
  static constexpr float default_port_y_offset = 2.0f;
  static constexpr float thin_tree_scale = 10.0f;
  static constexpr float thin_tree_scale_span = 2.0f;
  static constexpr float thick_tree_scale = 15.0f;
//  static constexpr double alive_duration_s = 120.0;
  static constexpr double alive_duration_s = 10.0;
//  static constexpr double leaf_particle_alive_time_s = 20.0;
  static constexpr double pollen_spawn_timeout_s = 15.0;
  static constexpr int max_num_pollen_particles = 20;
  static constexpr float medial_bud_angle_criterion = 0.8f;
  static constexpr Vec2f reverb_mix_limits{0.2f, 0.5f};
  static constexpr Vec2f reverb_fb_limits{0.0f, 0.5f};
  static constexpr bool enable_debug_attraction_points = true;
  static constexpr int max_num_attraction_points_per_tree = int(1e4);
  static constexpr float initial_attraction_points_span_size = 512.0f;
  static constexpr float max_attraction_points_span_size_split = 4.0f;
//  static constexpr float default_vine_radius = 0.04f;
  static constexpr float default_vine_radius = 0.03f;
  static constexpr int num_resource_spiral_particles_per_tree = 4;
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "ProceduralTreeComponent";
}

struct AllowTerminalBudSpawn {
public:
  explicit AllowTerminalBudSpawn(float medial_bud_angle_criterion) :
    medial_bud_angle_criterion{medial_bud_angle_criterion} {
    //
  }

  bool operator()(const tree::Internodes& inodes,
                  const tree::Bud& bud,
                  const Vec3f& shoot_dir) const;
public:
  float medial_bud_angle_criterion;
};

inline bool AllowTerminalBudSpawn::operator()(const tree::Internodes& inodes,
                                              const tree::Bud& bud,
                                              const Vec3f& shoot_dir) const {
  bool can_spawn = true;
  if (bud.is_terminal) {
    const auto& prev_dir = inodes[bud.parent].direction;
    can_spawn = dot(prev_dir, shoot_dir) >= medial_bud_angle_criterion;
  }
  return can_spawn;
}

using TreeMeta = ProceduralTreeComponent::TreeMeta;
using Tree = ProceduralTreeComponent::Tree;
using TreeState = ProceduralTreeComponent::TreeState;
using TreePhase = ProceduralTreeComponent::TreePhase;

using UpdateInfo = ProceduralTreeComponent::UpdateInfo;
using UpdateResult = ProceduralTreeComponent::UpdateResult;
using BeginUpdateInfo = ProceduralTreeComponent::BeginUpdateInfo;
using InitResult = ProceduralTreeComponent::InitResult;

using PendingPortPlacement = ProceduralTreeAudioNodes::PendingPortPlacement;

Vec3f random_tree_origin(const Vec3f& p, const Vec3f& s) {
  return p + Vec3f{urand_11f(), 0.0f, urand_11f()} * s;
}

Vec3f random_tree_origin(const ProceduralTreeComponent& component) {
  auto off = Vec3f{urand_11f(), 0.0f, urand_11f()} * component.new_tree_origin_span;
  return component.default_new_tree_origin + off;
}

int pine_attraction_points(Vec3f* dst, int max_count, const Vec3f& ori, float tree_scale) {
  auto scl = Vec3f{0.75f, 4.0f, 0.75f} * tree_scale;
  const int num_gen = std::min(max_count, int(1e4));
  points::uniform_hemisphere(dst, num_gen, scl, ori);
  return num_gen;
}

int low_to_ground_attraction_points(Vec3f* dst, int max_count, const Vec3f& ori, float tree_scale) {
  auto scl = Vec3f{2.0f, 4.0f, 2.0f} * tree_scale;
  const int num_gen = std::min(max_count, int(1e4));
  points::uniform_hemisphere(dst, num_gen, scl, ori);
  return num_gen;
}

int high_above_ground_attraction_points(Vec3f* dst, int max_count,
                                        const Vec3f& ori, float tree_scale) {
  auto scl = Vec3f{2.0f, 4.0f, 2.0f} * tree_scale;
  const int num_gen = std::min(max_count, int(1e4));
  points::uniform_cylinder_to_hemisphere(dst, num_gen, scl, ori);
  return num_gen;
}

int squat_attraction_points(Vec3f* dst, int max_count, const Vec3f& ori, float tree_scale) {
  auto scl = Vec3f{2.0f, 1.0f, 2.0f} * tree_scale;
  const int num_gen = std::min(max_count, int(1e4));
  points::uniform_hemisphere(dst, num_gen, scl, ori);
  return num_gen;
}

[[maybe_unused]]
void get_leaf_internode_bounds_scale_offset_original_distribution(
  float leaf_scale, Vec3f* scale, Vec3f* off) {
  //
  *scale = Vec3f{1.0f, 4.0f * leaf_scale, 1.0f};
  *off = Vec3f{0.0f, 4.0f * leaf_scale, 0.0f};
}

[[maybe_unused]]
void get_leaf_internode_bounds_scale_offset_outwards_distribution(Vec3f* scale, Vec3f* off) {
  *scale = Vec3f{2.0f};
  *off = Vec3f{3.0f, 0.0f, 3.0f};
}

float small_proc_leaf_scale() {
  return 0.25f;
}

float rand_small_proc_leaf_scale() {
  return 0.25f + urand_11f() * 0.05f;
}

float default_decide_leaf_scale() {
  auto scale_decider = urand();
  if (scale_decider >= 1.0/3.0 && scale_decider < 2.0/3.0) {
    return 1.0f;
  } else if (scale_decider >= 2.0/3.0) {
    return small_proc_leaf_scale();
  } else {
    return 0.65f;
  }
}

float decide_leaf_scale_thick() {
  return 1.0f;
}

tree::DistributeBudQParams make_distribute_bud_q_params() {
  return tree::DistributeBudQParams::make_debug();
}

tree::SpawnInternodeParams make_pine_spawn_params(float tree_scale) {
  auto spawn_p = tree::SpawnInternodeParams::make_pine(tree_scale);
  spawn_p.allow_spawn_func = [](const tree::Internodes& nodes,
                                const tree::Bud& bud, const Vec3f& dir) {
    if (nodes[bud.parent].gravelius_order == 0) {
      return AllowTerminalBudSpawn{Config::medial_bud_angle_criterion}(nodes, bud, dir);
    } else {
      return true;
    }
  };
  return spawn_p;
}

tree::SpawnInternodeParams make_thin_spawn_params(float tree_scale) {
  auto spawn_p = tree::SpawnInternodeParams::make_debug(tree_scale);
  spawn_p.allow_spawn_func = AllowTerminalBudSpawn{Config::medial_bud_angle_criterion};
  return spawn_p;
}

tree::SpawnInternodeParams make_thick_spawn_params(float tree_scale) {
  auto spawn_p = tree::SpawnInternodeParams::make_debug_thicker(tree_scale);
  spawn_p.allow_spawn_func = AllowTerminalBudSpawn{Config::medial_bud_angle_criterion};
  return spawn_p;
}

Tree make_tree(const Vec3f& ori, tree::TreeInstanceHandle instance,
               tree::RenderTreeInstanceHandle render_instance, TreeMeta&& meta) {
  Tree tree{};
  tree.origin = ori;
  tree.instance = instance;
  tree.render_instance = render_instance;
  tree.meta = std::move(meta);
  return tree;
}

ProceduralTreeComponent::TreePendingRemoval make_pending_removal(tree::TreeID id) {
  ProceduralTreeComponent::TreePendingRemoval result{};
  result.id = id;
  return result;
}

[[maybe_unused]] bool is_idle(TreePhase phase) {
  return phase == TreePhase::Idle;
}

bool is_idle(TreeState state) {
  return state == TreeState::Idle;
}

float get_axis_growth_increment(const ProceduralTreeComponent& component, tree::TreeID id,
                                bool instrument_control_by_environment, float adjust11) {
  auto growth_incr = component.axis_growth_incr;
  if (instrument_control_by_environment) {
    growth_incr = 0.0f;
    if (auto signal_value = component.audio_nodes.get_signal_value(id)) {
      growth_incr = signal_value.value() * component.signal_axis_growth_incr_scale;
    }
  }
  growth_incr = std::max(0.0f, growth_incr + adjust11 * growth_incr);
  return growth_incr;
}

bool need_set_axis_growth_increment(const tree::TreeSystem::ReadInstance& inst) {
  using ModifyingState = tree::TreeSystem::ModifyingState;
  auto& modifying = inst.growth_state.modifying;
  return modifying == ModifyingState::RenderGrowing ||
         modifying == ModifyingState::RenderDying ||
         modifying == ModifyingState::Pruning;
}

void set_axis_growth_increments(ProceduralTreeComponent& component, const BeginUpdateInfo& info) {
  assert(info.bpm11 >= -1.0f && info.bpm11 <= 1.0f);
  for (auto& [id, tree] : component.trees) {
    auto read_inst = tree::read_tree(info.tree_system, tree.instance);
    if (need_set_axis_growth_increment(read_inst)) {
      const float growth_incr = get_axis_growth_increment(
        component, id, component.axis_growth_by_signal, info.bpm11);
      tree::set_axis_growth_increment(info.tree_system, tree.instance, growth_incr);
    }
  }
}

void gather_instrument_changes(
  ProceduralTreeComponent& component, audio::NodeSignalValueSystem* node_signal_value_system) {
  //
  auto changes = component.tree_instrument.update();
  component.audio_nodes.process_monitorable_changes(node_signal_value_system, make_view(changes));
}

void update_signal_changes_to_leaves(ProceduralTreeComponent& component, const UpdateInfo& info) {
  for (auto& [id, tree] : component.trees) {
    if (auto signal_value = component.audio_nodes.get_signal_value(id)) {
      {
        float osc_scale = component.static_wind_fast_osc_amplitude_scale;
        float osc_dt = osc_scale * signal_value.value();
        tree::increment_static_leaf_uv_osc_time(info.render_tree_system, tree.render_instance, osc_dt);
      }
    }
#if 0
    if (auto note_num_value = component.audio_nodes.get_triggered_osc_note_number_value(id)) {
      const float uv_off = std::sin(1024.0f * note_num_value.value()) * 0.5f + 0.5f;
      auto& inst = tree.render_instance;
      tree::set_static_leaf_uv_offset_target(info.render_tree_system, inst, uv_off);
    }
#endif
  }
}

void spawn_particles_at_leaves(const tree::Internodes& internodes, int n, UpdateResult& out) {
  Temporary<Vec3f, 1024> store_leaf_pos;
  auto* leaf_tip_pos = store_leaf_pos.require(int(internodes.size()));

  int num_leaf_tip_pos{};
  for (auto& node : internodes) {
    if (node.is_leaf()) {
      leaf_tip_pos[num_leaf_tip_pos++] = node.tip_position();
    }
  }

  std::sort(leaf_tip_pos, leaf_tip_pos + num_leaf_tip_pos, [](const auto& a, const auto& b) {
    return a.y > b.y;
  });

  const int num_particles = std::min(n, num_leaf_tip_pos);
  for (int p = 0; p < num_particles; p++) {
    ProceduralTreeComponent::SpawnPollenParticle particle{};
    particle.position = leaf_tip_pos[p];
    particle.enable_tree_spawn = false;
    out.spawn_pollen_particles.push_back(particle);
  }
}

bool can_start_dying(const TreeMeta& meta, double alive_time) {
  return !meta.dying && is_idle(meta.tree_state) &&
          meta.finished_render_growth && alive_time > Config::alive_duration_s;
}

void start_dying(Tree& tree, const UpdateInfo& info) {
  assert(is_idle(tree.meta.tree_state) && is_idle(tree.meta.tree_phase));
  //  Start branch death.
  tree::start_render_dying(info.tree_system, tree.instance);
  tree::set_leaf_scale_target(info.render_tree_system, tree.render_instance, 0.0f);
  tree.meta.tree_state = TreeState::RenderDying;
  tree.meta.tree_phase = TreePhase::AwaitingFinishRenderDeath;
  tree.meta.need_start_dying = false;
  tree.meta.dying = true;
}

void update_health(ProceduralTreeComponent& component, const UpdateInfo& info, UpdateResult& out) {
  if (!component.can_trigger_death) {
    return;
  }

  for (auto& [tree_id, tree] : component.trees) {
    if (can_start_dying(tree.meta, tree.meta.alive_timer.delta().count())) {
#if 0
      const auto node_ori = tree.origin;
      const auto soil_p = Vec2f{node_ori.x, node_ori.z};
      const auto soil_health = info.soil.sample_quality01(soil_p, 4.0f);
      const bool do_trigger = soil_health.y < 0.25f;
#else
      const bool do_trigger = tree.meta.need_start_dying;
#endif
      if (do_trigger) {
        start_dying(tree, info);

        const auto read_inst = tree::read_tree(info.tree_system, tree.instance);
        if (read_inst.nodes) {
          spawn_particles_at_leaves(read_inst.nodes->internodes, 16, out);
        }

        out.num_began_dying++;
      }
    }
  }
}

void update_branch_swell(ProceduralTreeComponent& component,
                         const UpdateInfo& info, UpdateResult& out) {
  for (auto& [id, tree] : component.trees) {
    auto& meta = tree.meta;
    if (!meta.finished_growing) {
      //  Only swell if finished branch growth.
      continue;
    }
    auto& swell_info = meta.swell_info;
#if INCLUDE_SOIL
    auto node_ori = tree.origin;
    const Vec3f sampled = info.soil.sample_quality01(Vec2f{node_ori.x, node_ori.z}, 4.0f);
    float l = sampled[swell_info.sense_channel_index];
#else
    float l = 0.0f;
#endif
    if (!swell_info.triggered_swell) {
      if (l > 0.75f) {
        swell_info.triggered_swell = true;
      } else {
        l = 0.0f;
      }
    } else {
      swell_info.swell_fraction += swell_info.swell_incr;
      if (swell_info.swell_incr > 0.0f && swell_info.swell_fraction >= 1.0f) {
        //  Finished uptake
        swell_info.swell_fraction = 1.0f;
        swell_info.triggered_swell = false;
        swell_info.swell_incr = -swell_info.swell_incr;
        //  Spawn pollen particles from the N highest leaf tips.
        const auto read_inst = tree::read_tree(info.tree_system, tree.instance);
        if (meta.finished_render_growth && urand() > 0.5 && read_inst.nodes) {
          spawn_particles_at_leaves(read_inst.nodes->internodes, 4, out);
        }
      } else if (swell_info.swell_incr < 0.0f && swell_info.swell_fraction <= 0.0f) {
        //  Finished deposit
        swell_info.swell_fraction = 0.0f;
        swell_info.triggered_swell = false;
        swell_info.swell_incr = -swell_info.swell_incr;
        //
        ProceduralTreeComponent::SoilDeposit deposit{};
        auto tree_ori = tree.origin;
        deposit.amount = Vec3f{};
        deposit.amount[swell_info.deposit_channel_index] = 1.0f;
        deposit.position = Vec2f{tree_ori.x, tree_ori.z};
        deposit.radius = 16.0f;
        out.soil_deposits.push_back(deposit);
      }
    }
  }
}

void update_pollen(ProceduralTreeComponent& component, const UpdateInfo& info, UpdateResult& out) {
  for (auto& to_term : info.pollen_update_res.to_terminate) {
    if (component.tree_spawn_enabled &&
        component.active_pollen_particles.count(to_term.id) > 0 &&
        urand() > 0.95) {
      ProceduralTreeComponent::PendingNewTree pending{};
      pending.position = to_term.terminal_position;
      component.pending_new_trees.push_back(std::move(pending));
    }
    component.active_pollen_particles.erase(to_term.id);
  }

  for (auto& [id, tree] : component.trees) {
    auto& meta = tree.meta;
    if (meta.can_trigger_pollen_spawn && !meta.triggered_pollen_spawn) {
      auto elapsed = meta.pollen_spawn_timer.delta().count();
      if (elapsed > Config::pollen_spawn_timeout_s) {
        const auto read_inst = tree::read_tree(info.tree_system, tree.instance);
        if (!read_inst.nodes) {
          continue;
        }
        //  Spawn a pollen particle from the N highest leaf tips.
        auto leaf_tip_pos = tree::collect_leaf_tip_positions(read_inst.nodes->internodes);
        std::sort(leaf_tip_pos.begin(), leaf_tip_pos.end(), [](const auto& a, const auto& b) {
          return a.y > b.y;
        });
        auto num_particles = std::min(Config::max_num_pollen_particles, int(leaf_tip_pos.size()));
        for (int p = 0; p < num_particles; p++) {
          ProceduralTreeComponent::SpawnPollenParticle particle{};
          particle.position = leaf_tip_pos[p];
          particle.enable_tree_spawn = true;
          out.spawn_pollen_particles.push_back(particle);
        }
        meta.triggered_pollen_spawn = true;
      }

    } else if (meta.can_trigger_pollen_spawn && meta.triggered_pollen_spawn) {
#if 0 //  spawn pollen if signal > threshold, at some interval
      auto signal_value = component.audio_nodes.get_signal_value(id);
      if (!signal_value) {
        continue;
      }

      const auto read_inst = tree::read_tree(info.tree_system, tree.instance);
      if (!read_inst.nodes) {
        continue;
      }

      if (signal_value.value() <= 0.75f || meta.pollen_spawn_timer.delta().count() <= 2.0) {
        continue;
      }

      meta.pollen_spawn_timer.reset();
      const auto& inodes = read_inst.nodes->internodes;
      int i{};
      int ns{};
      while (i < int(inodes.size()) && ns < 8) {
        if (inodes[i].is_leaf() && urand() < 0.5) {
          ProceduralTreeComponent::SpawnPollenParticle particle{};
          particle.position = inodes[i].position;
          particle.enable_tree_spawn = false;
          out.spawn_pollen_particles.push_back(particle);
          ns++;
        }
        i++;
      }
#endif
    }
  }
}

void update_create_vines(ProceduralTreeComponent& component, const UpdateInfo& info) {
  const auto& node_id_map = component.audio_nodes.audio_node_id_to_tree_id;

  for (auto& connect : info.audio_connection_update_result.new_connections) {
    auto first_tree_id_it = node_id_map.find(connect.first.node_id);
    auto sec_tree_id_it = node_id_map.find(connect.second.node_id);
    if (first_tree_id_it == node_id_map.end() || sec_tree_id_it == node_id_map.end()) {
      continue;
    }

    auto tree_it = component.trees.find(first_tree_id_it->second);
    if (tree_it == component.trees.end()) {
      continue;
    }

    auto& tree = tree_it->second;
    if (tree.vine_instance) {
      //  already has vine
      continue;
    }

    auto inst = tree::create_vine_instance(info.vine_system, Config::default_vine_radius);
    tree.vine_instance = inst;
    const int n = 4;

    const float theta0 = pif() * 0.5f - pif() * 0.25f;
    const float theta1 = pif() * 0.5f + pif() * 0.25f;
    for (int i = 0; i < n; i++) {
//      const float theta = lerp(float(i) / float(n-1), theta0, theta1);
      const float theta = lerp(urandf(), theta0, theta1);
      auto segment = tree::start_new_vine_on_tree(info.vine_system, inst, tree.instance, theta);
      tree::VineSystemTryToJumpToNearbyTreeParams jump_params{};
      tree::try_to_jump_to_nearby_tree(info.vine_system, inst, segment, jump_params);
#if 1
      tree::create_ornamental_foliage_on_vine_segment(inst, segment);
#endif
    }
  }
}

void update_vine_growth_by_signal(ProceduralTreeComponent& component, const UpdateInfo& info) {
  for (auto& [tree_id, tree] : component.trees) {
    if (!tree.vine_instance) {
      continue;
    }

    float signal_val{0.0f};
    if (component.grow_vines_by_signal) {
      if (auto signal = component.audio_nodes.get_signal_value(tree_id)) {
        signal_val = std::max(0.0f, signal.value());
      }
    } else {
      signal_val = 1.0f;
    }

    tree::set_growth_rate_scale(info.vine_system, tree.vine_instance.value(), signal_val);
  }
}

void update_changes_due_to_season(ProceduralTreeComponent& component, const UpdateInfo& info) {
  auto& status = info.season_status;
  if (status.events.just_began_transition || status.events.just_jumped_to_state) {
    component.season_transition_timer.reset();
    for (auto& [_, tree] : component.trees) {
      tree.meta.time_to_season_transition = lerp(urandf(), 1.0f, 5.0f);
    }
  }

  float elapsed_time = float(component.season_transition_timer.delta().count());
  for (auto& [_, tree] : component.trees) {
    if (tree.meta.time_to_season_transition > 0.0f &&
        elapsed_time >= tree.meta.time_to_season_transition) {

      float target{};
      if (info.season_status.status.transitioning) {
        target = info.season_status.status.next == season::Season::Fall ? 1.0f : 0.0f;
      } else {
        target = info.season_status.status.current == season::Season::Fall ? 1.0f : 0.0f;
      }

      tree::set_frac_fall_target(info.render_tree_system, tree.render_instance, target);
      tree.meta.time_to_season_transition = 0.0f;
    }
  }
}

void choose_new_message_color(const msg::Message& msg, Vec3f* color, float* frac_color) {
  constexpr int num_colors = 7;
  const Vec3f colors[num_colors] = {
    Vec3f{1.0f, 1.0f, 0.0f},
    Vec3f{0.0f, 1.0f, 1.0f},
    Vec3f{1.0f, 0.0f, 1.0f},
    Vec3f{0.5f, 0.5f, 1.0f},
    Vec3f{0.5f, 1.0f, 0.5f},
    Vec3f{1.0f, 0.5f, 0.5f},
    Vec3f{1.0f}
  };

  auto curr_color = msg.data.read_vec3f();
  const Vec3f* sampled = uniform_array_sample(colors, num_colors);
  while (*sampled == curr_color) {
    sampled = uniform_array_sample(colors, num_colors);
  }

  *color = *sampled;
  *frac_color = float(int(sampled - colors)) / float(num_colors - 1);
}

void update_resource_flow_particles(ProceduralTreeComponent& component, const UpdateInfo&) {
  static_assert(Config::num_resource_spiral_particles_per_tree <= 4);
  for (auto& [_, tree] : component.trees) {
    tree::ResourceSpiralAroundNodesHandle handle{tree.meta.resource_spiral_handle_indices[0]};
    if (handle.is_valid() || !tree.meta.finished_growing) {
      continue;
    }
    for (int i = 0; i < Config::num_resource_spiral_particles_per_tree; i++) {
      auto* sys = tree::get_global_resource_spiral_around_nodes_system();
      tree::CreateResourceSpiralParams create_params{};
      create_params.theta_offset = float(i) * pif() * 0.1f;
      create_params.scale = 0.25f;
      create_params.burrows_into_target = true;
      create_params.linear_color = Vec3<uint8_t>{255};
      auto spiral_handle = tree::create_resource_spiral_around_tree(sys, tree.instance, create_params);
      tree.meta.resource_spiral_handle_indices[i] = spiral_handle.index;
    }
  }
}

void update_messages(ProceduralTreeComponent& component, const UpdateInfo& info) {
  using ActiveMessage = ProceduralTreeComponent::ActiveMessage;

  const auto param_writer_id = component.parameter_writer_id;

  auto msgs = tree::read_messages(info.tree_message_system);
  auto particles = component.message_particles.update(msgs, info.real_dt);
  (void) particles;

  DynamicArray<ActiveMessage, 8> still_present;
  for (auto& message : tree::get_messages(info.tree_message_system)) {
    for (auto& msg : component.active_messages) {
      if (message.message.id != msg.message_id) {
        continue;
      }

      if (message.events.just_reached_new_leaf) {
        Optional<tree::TreeID> associated_tree_id;
        for (auto& [tree_id, inst] : component.trees) {
          if (inst.instance == message.tree) {
            associated_tree_id = tree_id;
            break;
          }
        }

        if (associated_tree_id) {
          Vec3f new_color{};
          float new_color_frac{};
          choose_new_message_color(message.message, &new_color, &new_color_frac);
          message.message.data.write_vec3f(new_color);
#if 0
          auto it = component.audio_nodes.triggered_osc_nodes.find(associated_tree_id.value());
          if (it != component.audio_nodes.triggered_osc_nodes.end() &&
              it->second.semitone_offset_desc) {
            const auto val = make_interpolated_parameter_value_from_descriptor(
              it->second.semitone_offset_desc.value(), new_color_frac);
            param_system::ui_set_value(
              info.parameter_system,
              param_writer_id,
              it->second.semitone_offset_desc.value().ids,
              val);
          }
#else
          (void) param_writer_id;
#endif
        }
      }

      still_present.push_back(msg);
      break;
    }
  }

  component.active_messages = std::move(still_present);

  constexpr int target_num_messages = 8;
  if (component.active_messages.size() < target_num_messages) {
    const int num_add = int(target_num_messages - component.active_messages.size());
    int num_added{};
    for (auto& [tree_id, tree] : component.trees) {
      if (num_added >= num_add) {
        break;
      }
      auto inst = tree::read_tree(info.tree_system, tree.instance);
      const bool has_nodes = inst.nodes && !inst.nodes->internodes.empty();
      if (inst.bounds_element_id.is_valid() && has_nodes && urand() > 0.5) {
        auto* root_node = &inst.nodes->internodes[0];
        auto msg = tree::make_zero_message(1.0f, 2.0f);
        auto tree_msg = tree::make_tree_message(
          msg, tree.instance, root_node->id, root_node->position);
        tree::push_message(info.tree_message_system, tree_msg);

        ActiveMessage active{};
        active.message_id = tree_msg.message.id;
        component.active_messages.push_back(active);

        for (int i = 0; i < 16; i++) {
          auto part = tree::MessageParticles::make_default_particle(
            tree_msg.message.id, root_node->position);
          component.message_particles.push_particle(part);
        }

        num_added++;
      }
    }
  }
}

void maybe_start_growing(ProceduralTreeComponent& component, const UpdateInfo& info) {
  if (component.need_grow && can_grow(info.growth_system, component.growth_context)) {
    grow(info.growth_system, component.growth_context);
    component.need_grow = false;
  }
}

void state_pending_prepare_to_grow(ProceduralTreeComponent& component,
                                   Tree& tree, const UpdateInfo& info) {
  assert(is_idle(tree.meta.tree_phase));
  tree::TreeSystem::PrepareToGrowParams params{};
  params.max_num_internodes = Config::max_num_internodes;
  params.context = component.growth_context;
  tree::prepare_to_grow(info.tree_system, tree.instance, params);

  tree.meta.tree_state = TreeState::Growing;
  tree.meta.tree_phase = TreePhase::AwaitingFinishGrowth;
  component.need_grow = true;
}

void state_growing(ProceduralTreeComponent& component, tree::TreeID id,
                   Tree& tree, const tree::TreeSystem::ReadInstance& tree_inst,
                   const tree::ReadRenderTreeSystemInstance& render_inst,
                   const UpdateInfo& info, UpdateResult& out) {
  switch (tree.meta.tree_phase) {
    case TreePhase::AwaitingFinishGrowth: {
      if (tree_inst.events.just_started_awaiting_finish_growth_signal) {
        //  @NOTE: If `tree_inst.nodes` can be null here, we can add another phase to wait for the
        //  nodes to become available.
        assert(tree_inst.nodes);
        const int remove_n = Config::remove_if_fewer_than_n_internodes_after_growing;
        if (int(tree_inst.nodes->internodes.size()) < remove_n) {
          GROVE_LOG_SEVERE_CAPTURE_META("Rejecting tree.", logging_id());
          tree::finish_growing(info.tree_system, tree.instance);
          component.trees_pending_removal.push_back(make_pending_removal(id));
          tree.meta.tree_state = TreeState::PendingDeletion;
          tree.meta.tree_phase = TreePhase::Idle;
        } else {
          tree::require_drawables(info.render_tree_system, tree.render_instance);
          tree.meta.tree_phase = TreePhase::AwaitingInitialDrawableCreation;
        }
      }
      break;
    }
    case TreePhase::AwaitingInitialDrawableCreation: {
      if (render_inst.events.just_created_drawables) {
        assert(!tree.meta.finished_growing && !tree.meta.finished_render_growth);
        tree.meta.finished_growing = true;

        tree::finish_growing(info.tree_system, tree.instance);
        tree::start_render_growing(info.tree_system, tree.instance);

        if (tree.meta.ports_pending_placement) {
          out.pending_placement.push_back(std::move(tree.meta.ports_pending_placement));
          tree.meta.ports_pending_placement = nullptr;
        }

        component.newly_created.push_back(id);

        tree.meta.tree_state = TreeState::RenderGrowing;
        tree.meta.tree_phase = TreePhase::AwaitingFinishRenderGrowth;
      }
      break;
    }
    default: {
      assert(false);
    }
  }
}

void state_render_growing(ProceduralTreeComponent& component,
                          Tree& tree, const tree::TreeSystem::ReadInstance& tree_inst,
                          const tree::ReadRenderTreeSystemInstance& render_inst,
                          const UpdateInfo& info, UpdateResult& out) {
  switch (tree.meta.tree_phase) {
    case TreePhase::AwaitingFinishRenderGrowth: {
      if (tree_inst.events.just_started_awaiting_finish_render_growth_signal) {
        tree::set_leaf_scale_target(
          info.render_tree_system, tree.render_instance, tree.meta.canonical_leaf_scale);
        tree.meta.tree_phase = TreePhase::GrowingLeaves;
      }
      break;
    }
    case TreePhase::GrowingLeaves: {
      if (render_inst.events.just_reached_leaf_target_scale) {
        assert(!tree.meta.finished_render_growth);
        tree.meta.finished_render_growth = true;
        tree::finish_render_growing(info.tree_system, tree.instance);
        //  once all finished growing, add some foliage at the base of the tree.
        if (component.add_flower_patch_after_growing) {
          auto tree_ori = tree.origin;
          Vec2f tree_ori_xz{tree_ori.x, tree_ori.z};

          ProceduralTreeComponent::MakeOrnamentalFoliage make_foliage{};
          make_foliage.position = tree_ori_xz;
          out.new_ornamental_foliage_patches.push_back(make_foliage);
        }
        //  Start alive timer.
        tree.meta.alive_timer.reset();
        //  Start timer for pollen spawn.
        tree.meta.can_trigger_pollen_spawn = true;
        tree.meta.pollen_spawn_timer.reset();

        tree.meta.tree_state = TreeState::Idle;
        tree.meta.tree_phase = TreePhase::Idle;

        out.num_leaves_finished_growing++;
      }
      break;
    }
    default: {
      assert(false);
    }
  }
}

void state_render_dying(ProceduralTreeComponent& component, tree::TreeID id,
                        Tree& tree, const tree::TreeSystem::ReadInstance& tree_inst,
                        const tree::ReadRenderTreeSystemInstance&,
                        const UpdateInfo&, UpdateResult&) {
  switch (tree.meta.tree_phase) {
    case TreePhase::AwaitingFinishRenderDeath: {
      if (tree_inst.events.just_finished_render_death) {
        component.trees_pending_removal.push_back(make_pending_removal(id));
        tree.meta.tree_state = TreeState::PendingDeletion;
        tree.meta.tree_phase = TreePhase::Idle;
      }
      break;
    }
    default: {
      assert(false);
    }
  }
}

void state_pending_deletion(ProceduralTreeComponent&, tree::TreeID,
                            Tree& tree, const tree::TreeSystem::ReadInstance&) {
  assert(is_idle(tree.meta.tree_phase));
  (void) tree;
}

void state_pruning(ProceduralTreeComponent& component, tree::TreeID id,
                   Tree& tree, const tree::TreeSystem::ReadInstance& tree_inst,
                   const tree::ReadRenderTreeSystemInstance& render_inst,
                   const UpdateInfo& info, UpdateResult&) {
  switch (tree.meta.tree_phase) {
    case TreePhase::PruningLeaves: {
      if (render_inst.events.just_reached_leaf_target_scale) {
        tree::finish_pruning_leaves(info.tree_system, tree.instance);
        tree.meta.tree_phase = TreePhase::PruningInternodes;
      }
      break;
    }
    case TreePhase::PruningInternodes: {
      if (tree_inst.events.just_started_awaiting_finish_pruning_signal) {
        tree::require_drawables(info.render_tree_system, tree.render_instance);
        tree.meta.tree_phase = TreePhase::AwaitingPrunedDrawableCreation;
      }
      break;
    }
    case TreePhase::AwaitingPrunedDrawableCreation: {
      if (render_inst.events.just_created_drawables) {
        tree::set_leaf_scale_target(
          info.render_tree_system, tree.render_instance, tree.meta.canonical_leaf_scale);
        tree.meta.tree_phase = TreePhase::UnpruningLeaves;
      }
      break;
    }
    case TreePhase::UnpruningLeaves: {
      if (render_inst.events.just_reached_leaf_target_scale) {
        tree.meta.tree_phase = TreePhase::EvaluatingPrune;
      }
      break;
    }
    case TreePhase::EvaluatingPrune: {
      if (tree_inst.nodes) {
        const int inode_thresh = Config::remove_if_fewer_than_n_internodes_after_pruning;
        bool remove_tree = int(tree_inst.nodes->internodes.size()) < inode_thresh;
        auto next_state = TreeState::Idle;
        if (remove_tree) {
          component.trees_pending_removal.push_back(make_pending_removal(id));
          next_state = TreeState::PendingDeletion;
        }
        tree::finish_pruning(info.tree_system, tree.instance);
        tree.meta.tree_state = next_state;
        tree.meta.tree_phase = TreePhase::Idle;
      }
      break;
    }
    default: {
      assert(false);
    }
  }
}

void state_idle(ProceduralTreeComponent&,
                Tree& tree, const tree::TreeSystem::ReadInstance& tree_inst,
                const tree::ReadRenderTreeSystemInstance&,
                const UpdateInfo& info, UpdateResult&) {
  if (tree_inst.events.just_started_pruning) {
    tree::set_leaf_scale_target(info.render_tree_system, tree.render_instance, 0.0f);
    tree.meta.tree_state = TreeState::Pruning;
    tree.meta.tree_phase = TreePhase::PruningLeaves;
  }
}

void state_dispatch(ProceduralTreeComponent& component,
                    tree::TreeID id, ProceduralTreeComponent::Tree& tree,
                    const UpdateInfo& info, UpdateResult& out) {
  auto tree_inst = tree::read_tree(info.tree_system, tree.instance);
  auto render_inst = tree::read_instance(info.render_tree_system, tree.render_instance);
  switch (tree.meta.tree_state) {
    case TreeState::Idle: {
      state_idle(component, tree, tree_inst, render_inst, info, out);
      break;
    }
    case TreeState::PendingPrepareToGrow: {
      state_pending_prepare_to_grow(component, tree, info);
      break;
    }
    case TreeState::Growing: {
      state_growing(component, id, tree, tree_inst, render_inst, info, out);
      break;
    }
    case TreeState::RenderGrowing: {
      state_render_growing(component, tree, tree_inst, render_inst, info, out);
      break;
    }
    case TreeState::Pruning: {
      state_pruning(component, id, tree, tree_inst, render_inst, info, out);
      break;
    }
    case TreeState::RenderDying: {
      state_render_dying(component, id, tree, tree_inst, render_inst, info, out);
      break;
    }
    case TreeState::PendingDeletion: {
      state_pending_deletion(component, id, tree, tree_inst);
      break;
    }
    default: {
      assert(false);
    }
  }
}

void process_tree_state(ProceduralTreeComponent& component,
                        const UpdateInfo& info, UpdateResult& out) {
  for (auto& [id, tree] : component.trees) {
    state_dispatch(component, id, tree, info, out);
  }
}

void update_delay_nodes(ProceduralTreeComponent& component,
                        const UpdateInfo& update_info, UpdateResult&) {
  const AudioParameterWriterID param_writer_id = component.parameter_writer_id;
  for (auto& [tree_id, node_info] : component.audio_nodes.delay_nodes) {
    if (node_info.chorus_mix_param_ids) {
      //  Set chorus mix according to wind force.
      auto& ids = node_info.chorus_mix_param_ids.value();
      Vec2f pos_xz{node_info.position.x, node_info.position.z};
      auto wind_f = update_info.wind.wind_force01_no_spectral_influence(pos_xz);
      param_system::ui_set_value(
        update_info.parameter_system, param_writer_id, ids, make_float_parameter_value(wind_f));
    }
    if (node_info.noise_mix_param_ids) {
      //  Add noise if dying.
      const auto& meta = component.trees.at(tree_id).meta;
      if (meta.dying) {
        auto& ids = node_info.noise_mix_param_ids.value();
        param_system::ui_set_value(
          update_info.parameter_system, param_writer_id, ids, make_float_parameter_value(0.75f));
      }
    }
  }
}

void update_envelope_nodes(ProceduralTreeComponent& component,
                           const UpdateInfo& update_info, UpdateResult&) {
  const AudioParameterWriterID param_writer_id = component.parameter_writer_id;
  for (auto& [tree_id, node_info] : component.audio_nodes.envelope_nodes) {
    const auto& tree = component.trees.at(tree_id);
    if (node_info.amp_mod_descriptor) {
      auto& descr = node_info.amp_mod_descriptor.value();
      auto param_val = make_interpolated_parameter_value_from_descriptor(
        descr, tree.meta.swell_info.swell_fraction);
      param_system::ui_set_value(
        update_info.parameter_system, param_writer_id, descr.ids, param_val);
    }
  }
}

void update_reverb_nodes(ProceduralTreeComponent& component,
                         const UpdateInfo& update_info, UpdateResult&) {
  const AudioParameterWriterID param_writer_id = component.parameter_writer_id;
  for (auto& [tree_id, node_info] : component.audio_nodes.reverb_nodes) {
    if (node_info.mix_param_ids) {
      //  Set mix according to wind force and healthiness.
      auto& ids = node_info.mix_param_ids.value();
      Vec2f pos_xz{node_info.position.x, node_info.position.z};
      auto wind_f = update_info.wind.wind_force01_no_spectral_influence(pos_xz);
      const auto mix_limits = Config::reverb_mix_limits;
      const auto wind_mix_val = lerp(wind_f, mix_limits.x, mix_limits.y);
      //  Attenuate mix to 0 when unhealthy.
      const float unhealthiness = component.trees.at(tree_id).unhealthiness;
      const float mix_val = wind_mix_val * (1.0f - std::pow(unhealthiness, 8.0f));
      param_system::ui_set_value(
        update_info.parameter_system, param_writer_id, ids, make_float_parameter_value(mix_val));
    }
    if (node_info.fb_param_ids) {
      const float fb_frac = component.trees.at(tree_id).meta.swell_info.swell_fraction;
      auto& ids = node_info.fb_param_ids.value();
      const auto fb_limits = Config::reverb_fb_limits;
      const auto fb_value = lerp(fb_frac, fb_limits.x, fb_limits.y);
      param_system::ui_set_value(
        update_info.parameter_system, param_writer_id, ids, make_float_parameter_value(fb_value));
    }
    if (node_info.fixed_osc_mix_param_ids) {
      const auto& tree = component.trees.at(tree_id);
      const float val = tree.unhealthiness;
      auto& ids = node_info.fixed_osc_mix_param_ids.value();
      param_system::ui_set_value(
        update_info.parameter_system, param_writer_id, ids, make_float_parameter_value(val));
    }
  }
}

ProceduralTreeAudioNodes::Context make_audio_nodes_context(ProceduralTreeComponent& component,
                                                           const UpdateInfo& info) {
  return ProceduralTreeAudioNodes::Context{
    component.parameter_writer_id,
    info.node_storage,
    *info.parameter_system,
    info.audio_observation,
    info.audio_scale,
    component.tree_instrument
  };
}

void update_instruments(ProceduralTreeComponent& component,
                        const UpdateInfo& update_info, UpdateResult& out) {
  component.audio_nodes.gather_parameter_ids(make_audio_nodes_context(component, update_info));
  update_delay_nodes(component, update_info, out);
  update_envelope_nodes(component, update_info, out);
  update_reverb_nodes(component, update_info, out);
}

float canonical_leaf_scale(const ProceduralTreeComponent& component,
                           bool is_proc, bool is_thick_tree) {
  if (is_proc) {
    if (is_thick_tree) {
      return decide_leaf_scale_thick();
    } else {
      return component.always_small_proc_leaves ?
        rand_small_proc_leaf_scale() : default_decide_leaf_scale();
    }
  } else {
    if (is_thick_tree) {
      return 1.25f;
    } else {
      return 1.0f;
    }
  }
}

tree::TreeID add_tree(ProceduralTreeComponent& component, const Vec3f& pos, const UpdateInfo& info) {
  tree::CreateTreeParams params{};
  params.origin = pos;

  const auto use_spawn_params_type = component.spawn_params_type;
  const auto use_points_type = component.attraction_points_type;
  const bool is_thin_tree = use_spawn_params_type == 0;
  const auto tree_scale = [&]() {
    if (is_thin_tree) {
      return Config::thin_tree_scale + urand_11f() * Config::thin_tree_scale_span;
    } else {
      return Config::thick_tree_scale;
    }
  }();
  if (component.is_pine) {
    params.spawn_params = make_pine_spawn_params(tree_scale);
  } else {
    params.spawn_params = is_thin_tree ?
      make_thin_spawn_params(tree_scale) :
      make_thick_spawn_params(tree_scale);
  }
  params.bud_q_params = make_distribute_bud_q_params();
  if (component.is_pine) {
    params.make_attraction_points = [pos, tree_scale](Vec3f* dst, int max_count) {
      return pine_attraction_points(dst, max_count, pos, tree_scale);
    };
  } else {
    params.make_attraction_points = [use_points_type, pos, tree_scale](Vec3f* dst, int max_count) {
      if (use_points_type == 0) {
        return high_above_ground_attraction_points(dst, max_count, pos, tree_scale);
      } else if (use_points_type == 1) {
        return low_to_ground_attraction_points(dst, max_count, pos, tree_scale);
      } else {
        return squat_attraction_points(dst, max_count, pos, tree_scale);
      }
    };
  }

  const bool is_proc_leaves = false;

  TreeMeta meta{};
  meta.tree_state = TreeState::PendingPrepareToGrow;
  meta.canonical_leaf_scale = canonical_leaf_scale(component, is_proc_leaves, !is_thin_tree);

  params.insert_into_accel = info.insert_into_accel;
#if 1
  params.leaf_bounds_distribution_strategy =
    tree::TreeSystemLeafBoundsDistributionStrategy::AxisAlignedOutwardsFromNodes;
  get_leaf_internode_bounds_scale_offset_outwards_distribution(
    &params.leaf_internode_bounds_scale,
    &params.leaf_internode_bounds_offset);
#else
  get_leaf_internode_bounds_scale_offset_original_distribution(
    meta.canonical_leaf_scale,
    &params.leaf_internode_bounds_scale,
    &params.leaf_internode_bounds_offset);
#endif

  //  Swell info
  const auto soil_sense_channel_index = int8_t(urand() * 3.0);
  meta.swell_info.sense_channel_index = soil_sense_channel_index;
  meta.swell_info.deposit_channel_index = int8_t((soil_sense_channel_index + 1) % 3);

  tree::TreeID tree_id{};
  auto instance = tree::create_tree(info.tree_system, std::move(params), &tree_id);

  tree::CreateRenderTreeInstanceParams render_instance_params{};
  render_instance_params.tree = instance;
  render_instance_params.query_accel = info.insert_into_accel;

  if (!component.disable_foliage_components) {
    tree::CreateRenderFoliageParams create_foliage_component_params{};
    auto leaves_type = tree::CreateRenderFoliageParams::LeavesType::Maple;
    switch (component.foliage_leaves_type) {
      case 1:
        leaves_type = tree::CreateRenderFoliageParams::LeavesType::Willow;
        break;
      case 2:
        leaves_type = tree::CreateRenderFoliageParams::LeavesType::ThinCurled;
        break;
      case 3:
        leaves_type = tree::CreateRenderFoliageParams::LeavesType::Broad;
        break;
      default:
        break;
    }

    const auto& season_status = info.season_status.status;
    if ((!season_status.transitioning && season_status.current == season::Season::Fall) ||
        (season_status.transitioning && season_status.next == season::Season::Fall)) {
      create_foliage_component_params.init_with_fall_colors = true;
    }

    if (component.isolated_audio_node) {
      //  another node is currently isolated, so "hide" the new leaves by setting the initial
      //  scale to 0.
      create_foliage_component_params.init_with_zero_global_scale = true;
    }

    create_foliage_component_params.leaves_type = leaves_type;
    render_instance_params.create_foliage_components = create_foliage_component_params;
  }

#if 1
  render_instance_params.enable_branch_nodes_drawable_components = true;
#endif
  auto render_instance = tree::create_instance(info.render_tree_system, render_instance_params);

  auto tree = grove::make_tree(pos, instance, render_instance, std::move(meta));
  component.trees[tree_id] = std::move(tree);
  return tree_id;
}

tree::TreeID add_deserialized_tree(ProceduralTreeComponent& component,
                                   tree::TreeNodeStore&& nodes, const UpdateInfo& info) {
  //  @TODO
  auto tree_id = add_tree(component, nodes.origin(), info);
  auto& tree = component.trees.at(tree_id);
  nodes.id = tree_id;
  //  tree.nodes = std::move(nodes);
  tree.meta.deserialized = true;
  return tree_id;
}

void maybe_add_trees(ProceduralTreeComponent& component, const UpdateInfo& info) {
  auto& pending_new_trees = component.pending_new_trees;
  auto& trees = component.trees;

  while (!pending_new_trees.empty()) {
    auto pend = std::move(pending_new_trees.back());
    pending_new_trees.pop_back();

    tree::TreeID tree_id;
    Vec3f tree_pos;
    if (pend.deserialized) {
      tree_pos = pend.deserialized->origin();
      tree_id = add_deserialized_tree(component, std::move(*pend.deserialized), info);
      pend.deserialized = nullptr;
    } else {
      const float tree_height = info.terrain.height_nearest_position_xz(pend.position);
      tree_pos = Vec3f{pend.position.x, tree_height, pend.position.z};
      tree_id = add_tree(component, tree_pos, info);
    }

    auto& tree_meta = trees.at(tree_id).meta;
    AudioNodeStorage::NodeID node_id{};
    (void) node_id;
    auto node_pos = tree_pos + Vec3f{1.0f, Config::default_port_y_offset, 1.0f};

    std::unique_ptr<PendingPortPlacement> pend_placement{};
    const auto ctx = make_audio_nodes_context(component, info);
    auto& audio_nodes = component.audio_nodes;

    const auto instr_decider = (component.next_audio_node_type++) % 3;
//    const auto instr_decider = 1;
    switch (instr_decider) {
      case 0:
        pend_placement = std::make_unique<PendingPortPlacement>(
          audio_nodes.create_reverb_node(ctx, tree_id, node_pos, Config::default_port_y_offset));
        break;
      case 1:
        pend_placement = std::make_unique<PendingPortPlacement>(
          audio_nodes.create_envelope_node(ctx, tree_id, node_pos, Config::default_port_y_offset));
        break;
#if 0
      case 2:
        pend_placement = std::make_unique<PendingPortPlacement>(
          audio_nodes.create_delay_node(ctx, tree_id, node_pos, Config::default_port_y_offset));
        break;
#endif
      default:
        pend_placement = std::make_unique<PendingPortPlacement>(
          audio_nodes.create_triggered_osc_node(ctx, tree_id, node_pos, Config::default_port_y_offset));
        break;
    }

    node_id = pend_placement->node_id;
    tree_meta.ports_pending_placement = std::move(pend_placement);
    assert(tree_meta.ports_pending_placement);
  }
}

void maybe_destroy_vine_instance(Tree& tree, tree::VineSystem* sys) {
  if (tree.vine_instance) {
    tree::destroy_vine_instance(sys, tree.vine_instance.value());
    tree.vine_instance = NullOpt{};
  }
}

void maybe_destroy_resource_spiral_instances(Tree& tree) {
  if (tree.meta.resource_spiral_handle_indices[0] > 0) {
    auto* sys = tree::get_global_resource_spiral_around_nodes_system();
    for (int i = 0; i < Config::num_resource_spiral_particles_per_tree; i++) {
      auto handle = tree::ResourceSpiralAroundNodesHandle{tree.meta.resource_spiral_handle_indices[i]};
      tree::destroy_resource_spiral(sys, handle);
    }
    tree.meta.resource_spiral_handle_indices[0] = 0;
  }
}

void maybe_remove_trees(ProceduralTreeComponent& component,
                        const UpdateInfo& info, UpdateResult& out) {
  auto& trees_pending_removal = component.trees_pending_removal;
  auto& trees = component.trees;

  while (!trees_pending_removal.empty()) {
    auto pend = trees_pending_removal.back();
    trees_pending_removal.pop_back();

    component.newly_destroyed.push_back(pend.id);

    auto tree_it = trees.find(pend.id);
    assert(tree_it != trees.end());
    auto& tree = tree_it->second;
    bool remove_placed_node = !tree.meta.ports_pending_placement;

    auto rem = component.audio_nodes.destroy_node(
      make_audio_nodes_context(component, info), pend.id, remove_placed_node);

    for (auto& rel : rem.release_parameter_writes) {
      out.release_parameter_writes.push_back(rel);
    }
    out.nodes_to_delete.push_back(rem.pending_deletion);

    tree::destroy_tree(info.tree_system, tree.instance);
    tree::destroy_instance(info.render_tree_system, tree.render_instance);
    maybe_destroy_resource_spiral_instances(tree);
    maybe_destroy_vine_instance(tree, info.vine_system);
    trees.erase(tree_it);
  }
}

void add_tree_at_tform_position(ProceduralTreeComponent& component, const UpdateInfo& info) {
  auto* place_tree_tform_instance = component.place_tree_tform_instance;
  Vec2f trans_xz = exclude(place_tree_tform_instance->get_current().translation, 1);
  ProceduralTreeComponent::PendingNewTree pend{};
  pend.position = Vec3f{trans_xz.x, info.terrain.height_nearest_position(trans_xz), trans_xz.y};
  component.pending_new_trees.push_back(std::move(pend));
}

void reset_tform_position(ProceduralTreeComponent& component, const UpdateInfo& info) {
  auto curr = component.place_tree_tform_instance->get_current();
  auto cam_p = info.camera.get_position() + info.camera.get_front() * 32.0f;
  auto h = info.terrain.height_nearest_position_xz(cam_p);
  curr.translation = Vec3f{cam_p.x, h + 4.0f, cam_p.z};
  component.place_tree_tform_instance->set(curr);
};

bool prune_selected_axis_index(const ProceduralTreeComponent& component, const UpdateInfo& info) {
  if (!component.selected_tree) {
    return false;
  }
  auto tree_it = component.trees.find(component.selected_tree.value());
  if (tree_it == component.trees.end()) {
    return false;
  }
  const auto instance_handle = tree_it->second.instance;
  if (!tree::can_start_pruning(info.tree_system, instance_handle)) {
    return false;
  }
  auto read_inst = tree::read_tree(info.tree_system, instance_handle);
  if (!read_inst.nodes) {
    return false;
  }

  auto& src_nodes = read_inst.nodes->internodes;
  auto accept = std::make_unique<bool[]>(src_nodes.size());
  std::fill(accept.get(), accept.get() + src_nodes.size(), true);

  const int prune_index = component.prune_selected_axis_index.value();
  int root_index{};
  bool found_root{};
  for (int i = 0; i < int(src_nodes.size()); i++) {
    if (src_nodes[i].is_axis_root(src_nodes) && root_index++ == prune_index) {
      accept[i] = false;
      found_root = true;
      break;
    }
  }
  if (!found_root) {
    return false;
  }

  tree::TreeSystem::PruningInternodes pruning_internodes{};
  pruning_internodes.internodes.resize(src_nodes.size());
  pruning_internodes.dst_to_src.resize(src_nodes.size());
  pruning_internodes.internodes.resize(tree::prune_rejected_axes(
    src_nodes.data(),
    accept.get(),
    int(src_nodes.size()),
    pruning_internodes.internodes.data(),
    pruning_internodes.dst_to_src.data()));
  pruning_internodes.dst_to_src.resize(pruning_internodes.internodes.size());

  tree::TreeSystem::PruningData pruning{};
  pruning.internodes = std::move(pruning_internodes);
  tree::start_pruning(info.tree_system, instance_handle, std::move(pruning));
  return true;
}

void handle_serialization(ProceduralTreeComponent& component, const UpdateInfo& info) {
  if (component.serialize_selected_to_file_path && component.selected_tree) {
    auto& trees = component.trees;
    if (auto it = trees.find(component.selected_tree.value()); it != trees.end()) {
      auto read_inst = tree::read_tree(info.tree_system, it->second.instance);
      if (read_inst.nodes) {
        tree::serialize_file(
          *read_inst.nodes,
          component.serialize_selected_to_file_path.value().c_str());
        component.serialize_selected_to_file_path = NullOpt{};
      }
    }
  }
}

} //  anon

InitResult ProceduralTreeComponent::initialize(const InitInfo& init_info) {
  InitResult result{};
  grow_vines_by_signal = false;

  add_flower_patch_after_growing = false;

#if 1
  disable_static_leaves = true;
#endif

  {
    tree::CreateGrowthContextParams params{};
    params.max_num_attraction_points_per_tree = Config::max_num_attraction_points_per_tree;
    params.max_attraction_point_span_size_split = Config::max_attraction_points_span_size_split;
    params.initial_attraction_point_span_size = Config::initial_attraction_points_span_size;
    growth_context = create_growth_context(init_info.growth_system, params);
  }

  parameter_writer_id = AudioParameterWriteAccess::create_writer();
  if (Config::enable_debug_attraction_points) {
    tree::debug::create_debug_growth_context_instance(growth_context);
  }

  int init_num_trees = Config::initial_num_trees;
  if (init_info.initial_num_trees >= 0) {
    init_num_trees = init_info.initial_num_trees;
  }

  if (init_num_trees == 1) {
    new_tree_origin_span = 0.0f;
  }

  for (int i = 0; i < init_num_trees; i++) {
    PendingNewTree pend{};
    pend.position = random_tree_origin(*this);
    pending_new_trees.push_back(std::move(pend));
  }

  if (true) {
    result.key_listener = [this, keyboard = &init_info.keyboard](
      const KeyTrigger::KeyState& pressed, const KeyTrigger::KeyState&) {
      if (keyboard->is_pressed(Key::LeftShift) && keyboard->is_pressed(Key::LeftAlt)) {
        if (pressed.count(Key::P)) {
          need_reset_tform_position = true;
        }
      }
    };
  }

//  randomize_static_or_proc_leaves = true;
  use_static_leaves = true;
//  is_pine = true;
//  axis_growth_by_signal = false;

  place_tree_tform_instance = init_info.place_tree_tform_instance;
  can_trigger_death = true;
  return result;
}

void ProceduralTreeComponent::begin_update(const BeginUpdateInfo& info) {
  gather_instrument_changes(*this, info.node_signal_value_system);
  set_axis_growth_increments(*this, info);
}

ProceduralTreeComponent::UpdateResult
ProceduralTreeComponent::update(const UpdateInfo& update_info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("ProceduralTreeComponent/update");
  UpdateResult result;

  newly_created.clear();
  newly_destroyed.clear();

  if (need_add_tree_at_tform_position) {
    add_tree_at_tform_position(*this, update_info);
    need_add_tree_at_tform_position = false;
  }
  if (prune_selected_axis_index && grove::prune_selected_axis_index(*this, update_info)) {
    prune_selected_axis_index = NullOpt{};
  }
  if (need_reset_tform_position) {
    reset_tform_position(*this, update_info);
    need_reset_tform_position = false;
  }

  handle_serialization(*this, update_info);

  maybe_add_trees(*this, update_info);
  maybe_remove_trees(*this, update_info, result);

  process_tree_state(*this, update_info, result);
  maybe_start_growing(*this, update_info);

  update_signal_changes_to_leaves(*this, update_info);
  update_health(*this, update_info, result);
  update_instruments(*this, update_info, result);
  update_messages(*this, update_info);
  update_resource_flow_particles(*this, update_info);
  update_pollen(*this, update_info, result);
  update_branch_swell(*this, update_info, result);
  update_create_vines(*this, update_info);
  update_vine_growth_by_signal(*this, update_info);
  update_changes_due_to_season(*this, update_info);

  return result;
}

void ProceduralTreeComponent::register_pollen_particle(PollenParticleID id) {
  active_pollen_particles.insert(id);
}

void ProceduralTreeComponent::set_healthiness(tree::TreeID tree, float h01) {
  assert(h01 >= 0.0f && h01 <= 1.0f);
  if (auto it = trees.find(tree); it != trees.end()) {
    it->second.unhealthiness = clamp01(1.0f - h01);
  }
}

void ProceduralTreeComponent::set_need_start_dying(tree::TreeID tree) {
  auto it = trees.find(tree);
  if (it != trees.end()) {
    it->second.meta.need_start_dying = true;
  }
}

Vec3f ProceduralTreeComponent::centroid_of_tree_origins() const {
  Vec3f o{};
  float ct{};
  for (auto& [_, tree] : trees) {
    o += tree.origin;
    ct += 1.0f;
  }
  if (ct > 0.0f) {
    o /= ct;
  }
  return o;
}

const ProceduralTreeComponent::Trees* ProceduralTreeComponent::maybe_read_trees() const {
  return &trees;
}

ArrayView<const tree::TreeID> ProceduralTreeComponent::read_newly_created() const {
  return make_view(newly_created);
}

ArrayView<const tree::TreeID> ProceduralTreeComponent::read_newly_destroyed() const {
  return make_view(newly_destroyed);
}

void ProceduralTreeComponent::create_tree(bool at_tform_pos) {
  PendingNewTree pend{};
  if (at_tform_pos) {
    auto rand_off = Vec3f{urand_11f(), 0.0f, urand_11f()} * new_tree_origin_span;
    auto off_xz = place_tree_tform_instance->get_current().translation;
    pend.position = rand_off + Vec3f{off_xz.x, 0.0f, off_xz.z};
  } else {
    pend.position = random_tree_origin(*this);
  }

  if (!disable_restricting_tree_origins_to_within_world_bound) {
    float ori_xz_dist = Vec2f{pend.position.x, pend.position.z}.length();
    if (ori_xz_dist > Terrain::terrain_dim * 0.5f - 32.0f) {
      return;
    }
  }

  pending_new_trees.push_back(std::move(pend));
}

Vec3f ProceduralTreeComponent::get_place_tform_translation() const {
  return place_tree_tform_instance->get_current().translation;
}

bool ProceduralTreeComponent::any_growing() const {
  for (auto& [_, tree] : trees) {
    if (!tree.meta.finished_growing) {
      return true;
    }
  }
  return false;
}

void ProceduralTreeComponent::evaluate_audio_node_isolator_update_result(
  tree::RenderTreeSystem* render_tree_system,
  AudioNodeStorage::NodeID newly_activated,
  AudioNodeStorage::NodeID newly_deactivated) {
  //
  if (newly_activated == 0 && newly_deactivated == 0) {
    return;
  }

  const auto get_tree_id = [this](uint32_t node) -> Optional<tree::TreeID> {
    if (node > 0) {
      auto it = audio_nodes.audio_node_id_to_tree_id.find(node);
      if (it != audio_nodes.audio_node_id_to_tree_id.end()) {
        return Optional<tree::TreeID>(it->second);
      }
    }
    return NullOpt{};
  };

  const Optional<tree::TreeID> act_id = get_tree_id(newly_activated);

  if (newly_deactivated > 0 && isolated_audio_node) {
    assert(newly_deactivated == isolated_audio_node.value());
    isolated_audio_node = NullOpt{};

    for (auto& [tree_id, tree] : trees) {
      tree::set_leaf_global_scale_fraction(render_tree_system, tree.render_instance, 1.0f);
    }
  }

  if (newly_activated > 0 && act_id) {
    isolated_audio_node = newly_activated;

    for (auto& [tree_id, tree] : trees) {
      const float s = tree_id == act_id.value() ? 1.0f : 0.0f;
      tree::set_leaf_global_scale_fraction(render_tree_system, tree.render_instance, s);
    }
  }
}

void ProceduralTreeComponent::on_gui_update(const ProceduralTreeGUI::GUIUpdateResult& res) {
  if (res.make_new_tree) {
    for (int i = 0; i < num_trees_manually_add; i++) {
      PendingNewTree pend{};
      pend.position = random_tree_origin(*this);
      pending_new_trees.push_back(std::move(pend));
    }
  }
  if (res.make_trees_at_origin) {
    for (int i = 0; i < 100; i++) {
      PendingNewTree pend{};
      pend.position = random_tree_origin(Vec3f{}, Vec3f{128.0f});
      pending_new_trees.push_back(std::move(pend));
    }
  }
  if (res.add_tree_at_tform_position) {
    need_add_tree_at_tform_position = true;
  }
  if (res.remake_drawables) {
#if 0
    //  @TODO
    for (auto& [_, tree] : trees) {
      tree.meta.need_create_drawables = true;
    }
#endif
  }
  if (res.tree_origin) {
    default_new_tree_origin = res.tree_origin.value();
  }
  if (res.tree_origin_span) {
    new_tree_origin_span = res.tree_origin_span.value();
  }
  if (res.add_flower_patch_after_growing) {
    add_flower_patch_after_growing = res.add_flower_patch_after_growing.value();
  }
  if (res.vine_growth_by_signal) {
    grow_vines_by_signal = res.vine_growth_by_signal.value();
  }
  if (res.tree_spawn_enabled) {
    tree_spawn_enabled = res.tree_spawn_enabled.value();
  }
  if (res.wind_influence_enabled) {
    wind_influence_enabled = res.wind_influence_enabled.value();
  }
  if (res.render_attraction_points) {
    tree::debug::set_debug_growth_context_point_drawable_active(
      growth_context,
      res.render_attraction_points.value());
  }
  if (res.render_node_skeleton) {
    render_node_skeleton = res.render_node_skeleton.value();
  }
  if (res.selected_tree) {
    selected_tree = res.selected_tree.value();
  }
  if (res.attraction_points_type) {
    attraction_points_type = res.attraction_points_type.value();
  }
  if (res.spawn_params_type) {
    spawn_params_type = res.spawn_params_type.value();
  }
  if (res.is_pine) {
    is_pine = res.is_pine.value();
  }
  if (res.foliage_leaves_type) {
    foliage_leaves_type = res.foliage_leaves_type.value();
  }
  if (res.axis_growth_incr) {
    axis_growth_incr = res.axis_growth_incr.value();
  }
  if (res.axis_growth_by_signal) {
    axis_growth_by_signal = res.axis_growth_by_signal.value();
  }
  if (res.can_trigger_death) {
    can_trigger_death = res.can_trigger_death.value();
  }
  if (res.proc_wind_fast_osc_scale) {
    proc_wind_fast_osc_amplitude_scale = res.proc_wind_fast_osc_scale.value();
  }
  if (res.static_wind_fast_osc_scale) {
    static_wind_fast_osc_amplitude_scale = res.static_wind_fast_osc_scale.value();
  }
  if (res.randomize_static_or_proc_leaves) {
    randomize_static_or_proc_leaves = res.randomize_static_or_proc_leaves.value();
  }
  if (res.use_static_leaves) {
    use_static_leaves = res.use_static_leaves.value();
  }
  if (res.disable_static_leaves) {
    disable_static_leaves = res.disable_static_leaves.value();
  }
  if (res.disable_foliage_components) {
    disable_foliage_components = res.disable_foliage_components.value();
  }
  if (res.use_hemisphere_color_image) {
    use_hemisphere_color_image = res.use_hemisphere_color_image.value();
  }
  if (res.randomize_hemisphere_color_images) {
    randomize_hemisphere_color_images = res.randomize_hemisphere_color_images.value();
  }
  if (res.always_small_proc_leaves) {
    always_small_proc_leaves = res.always_small_proc_leaves.value();
  }
  if (res.signal_axis_growth_scale) {
    signal_axis_growth_incr_scale = res.signal_axis_growth_scale.value();
  }
  if (res.signal_leaf_growth_scale) {
    signal_leaf_growth_incr_scale = res.signal_leaf_growth_scale.value();
  }
  if (res.num_trees_manually_add) {
    num_trees_manually_add = std::max(1, res.num_trees_manually_add.value());
  }
  if (res.serialize_selected_to_file_path) {
    serialize_selected_to_file_path = res.serialize_selected_to_file_path.value();
  }
  if (res.deserialized_tree_translation) {
    deserialized_tree_translation = res.deserialized_tree_translation.value();
  }
  if (res.deserialize_from_file_path) {
    auto deser_res = tree::deserialize_file(
      res.deserialize_from_file_path.value().c_str());
    if (deser_res) {
      PendingNewTree pend{};
      pend.deserialized = std::make_unique<tree::TreeNodeStore>(std::move(deser_res.value()));
      pend.deserialized->translate(deserialized_tree_translation);
      pending_new_trees.push_back(std::move(pend));
    }
  }
  if (res.prune_selected_axis_index) {
    prune_selected_axis_index = res.prune_selected_axis_index.value();
  }
  if (res.hide_foliage_drawable_components) {
    hide_foliage_drawable_components = res.hide_foliage_drawable_components.value();
  }
  if (res.resource_spiral_vel) {
    resource_spiral_global_particle_velocity = res.resource_spiral_vel.value();
  }
  if (res.resource_spiral_theta) {
    resource_spiral_global_particle_theta = res.resource_spiral_theta.value();
  }
}

GROVE_NAMESPACE_END
