#include "debug_health.hpp"
#include "ProceduralTreeComponent.hpp"
#include "resource_flow_along_nodes.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/math/ease.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr int max_num_spirals_per_event = 6;
  static constexpr float max_resource_spiral_scale = 2.0f;
};

enum class HealthEventState {
  Idle = 0,
  FallingIll,
  Dying,
  WillTriggerDeath,
  TriggeredDeath,
};

struct DiseaseEvent {
  Vec2f position;
  float radius;
};

struct DiseaseInfo {
  tree::ResourceSpiralAroundNodesHandle spirals[Config::max_num_spirals_per_event];
  int num_spirals;
  HealthEventState state;
  double t0;
};

struct DebugHealthSystem {
  Optional<DiseaseEvent> pending_disease_event;
  Optional<DiseaseEvent> active_disease_event;
  std::unordered_map<uint32_t, DiseaseInfo> disease_infos;
  Vec2f next_disease_event_position{};
  float next_disease_event_radius{32.0f};
  bool prefer_place_position{true};

  Stopwatch state_timer;
};

void create_dying_spirals(
  DiseaseInfo& info, const ProceduralTreeComponent::Tree& tree,
  tree::ResourceSpiralAroundNodesSystem* sys) {
  //
  assert(info.num_spirals == 0);
  for (int i = 0; i < Config::max_num_spirals_per_event; i++) {
    tree::CreateResourceSpiralParams spiral_params{};
    spiral_params.global_param_set_index = 2;
    spiral_params.linear_color = Vec3<uint8_t>{};
    spiral_params.non_fixed_parent_origin = true;
    spiral_params.burrows_into_target = true;
    spiral_params.scale = 0.0f;
    spiral_params.theta_offset = float(i) * pif() * 0.1f;
    auto spiral = tree::create_resource_spiral_around_tree(sys, tree.instance, spiral_params);
    info.spirals[info.num_spirals++] = spiral;
  }
}

void destroy_dying_spirals(DiseaseInfo& info, tree::ResourceSpiralAroundNodesSystem* sys) {
  for (int i = 0; i < info.num_spirals; i++) {
    tree::destroy_resource_spiral(sys, info.spirals[i]);
  }
  info.num_spirals = 0;
}

void set_dying_spiral_scale(
  DiseaseInfo& info, tree::ResourceSpiralAroundNodesSystem* sys, float s) {
  for (int i = 0; i < info.num_spirals; i++) {
    tree::set_resource_spiral_scale(sys, info.spirals[i], s);
  }
}

void set_dying_spiral_velocity_scale(
  DiseaseInfo& info, tree::ResourceSpiralAroundNodesSystem* sys, float s) {
  for (int i = 0; i < info.num_spirals; i++) {
    tree::set_resource_spiral_velocity_scale(sys, info.spirals[i], s);
  }
}

DiseaseInfo* require_disease_info(DebugHealthSystem& sys, uint32_t tree_id) {
  auto disease_it = sys.disease_infos.find(tree_id);
  if (disease_it == sys.disease_infos.end()) {
    DiseaseInfo disease_info{};
    sys.disease_infos[tree_id] = disease_info;
    disease_it = sys.disease_infos.find(tree_id);
  }
  return &disease_it->second;
}

void clear_disease_infos(
  DebugHealthSystem& sys, tree::ResourceSpiralAroundNodesSystem* resource_spiral_sys) {
  //
  for (auto& [_, disease_info] : sys.disease_infos) {
    destroy_dying_spirals(disease_info, resource_spiral_sys);
  }
  sys.disease_infos.clear();
}

struct {
  DebugHealthSystem sys;
} globals;

} //  anon

void tree::update_debug_health(const DebugHealthUpdateInfo& info) {
  auto* trees = info.proc_tree_component.maybe_read_trees();
  if (!trees) {
    return;
  }

  auto& sys = globals.sys;
  if (sys.pending_disease_event && !sys.active_disease_event) {
    sys.active_disease_event = sys.pending_disease_event.value();
    sys.pending_disease_event = NullOpt{};
    sys.state_timer.reset();
  }

  const Vec3f place_p = info.proc_tree_component.get_place_tform_translation();
  const Vec2f place_p_xz = Vec2f{place_p.x, place_p.z};

  if (!sys.active_disease_event) {
    clear_disease_infos(sys, info.resource_spiral_sys);

  } else {
    const DiseaseEvent evt = sys.active_disease_event.value();
    const double active_t = sys.state_timer.delta().count();

    const float min_health_falling_ill = 0.125f;

    for (auto& [tree_id, tree]: *trees) {
      if (!tree.is_fully_grown()) {
        continue;
      }

      const auto evt_p = sys.prefer_place_position ? place_p_xz : evt.position;
      const auto p_xz = Vec2f{tree.origin.x, tree.origin.z};
      auto v = p_xz - evt_p;
      const float dist = v.length();
      if (dist > evt.radius) {
        continue;
      }

      DiseaseInfo& disease_info = *require_disease_info(sys, tree_id.id);
      switch (disease_info.state) {
        case HealthEventState::Idle: {
          assert(disease_info.num_spirals == 0);
          create_dying_spirals(disease_info, tree, info.resource_spiral_sys);
          disease_info.state = HealthEventState::FallingIll;
          disease_info.t0 = sys.state_timer.delta().count();
          break;
        }
        case HealthEventState::FallingIll: {
          double frac_ill = clamp01(clamp(active_t - disease_info.t0, 0.0, 20.0) / 20.0);
          double lerp_frac = ease::in_out_expo(frac_ill);
          const float inv_frac = 1.0f - float(lerp_frac);

          info.proc_tree_component.set_healthiness(
            tree_id, min_health_falling_ill + (1.0f - min_health_falling_ill) * inv_frac);
          set_dying_spiral_scale(
            disease_info, info.resource_spiral_sys,
            float(lerp_frac * Config::max_resource_spiral_scale));

          if (frac_ill == 1.0) {
            disease_info.t0 = sys.state_timer.delta().count();
            disease_info.state = HealthEventState::Dying;
          }
          break;
        }
        case HealthEventState::Dying: {
          //  Want trigger death -> possibly give other systems opportunity to intercede
          if (active_t - disease_info.t0 > 10.0) {
            disease_info.t0 = sys.state_timer.delta().count();
            disease_info.state = HealthEventState::WillTriggerDeath;
          }
          break;
        }
        case HealthEventState::WillTriggerDeath: {
          double frac_ill = clamp01(clamp(active_t - disease_info.t0, 0.0, 1.0) / 1.0);
          double lerp_frac = ease::in_out_expo(frac_ill);
          const float inv_frac = 1.0f - float(lerp_frac);

          info.proc_tree_component.set_healthiness(tree_id, min_health_falling_ill * inv_frac);
          set_dying_spiral_scale(
            disease_info, info.resource_spiral_sys,
            Config::max_resource_spiral_scale + float(lerp_frac * 1.0f));

          if (frac_ill == 1.0) {
            info.proc_tree_component.set_need_start_dying(tree_id);
            disease_info.t0 = sys.state_timer.delta().count();
            disease_info.state = HealthEventState::TriggeredDeath;
          }
          break;
        }
        case HealthEventState::TriggeredDeath: {
          double frac_till_trigger = clamp01(clamp(active_t - disease_info.t0, 0.0, 2.0) / 2.0);
          double frac_till_slow = clamp01(clamp(active_t - disease_info.t0, 0.0, 2.0) / 2.0);

          double lerp_frac = std::sqrt(frac_till_slow);
          set_dying_spiral_velocity_scale(
            disease_info, info.resource_spiral_sys, float(-1.0f * lerp_frac));

          if (frac_till_trigger == 1.0) {
            destroy_dying_spirals(disease_info, info.resource_spiral_sys);
          }
          break;
        }
      }
    }

    if (active_t > 40.0) {
      clear_disease_infos(sys, info.resource_spiral_sys);
      sys.active_disease_event = NullOpt{};
    }
  }
}

void tree::render_debug_health_gui() {
  auto& sys = globals.sys;
  ImGui::Begin("DebugHealth");

  ImGui::InputFloat2("NextDiseaseEventP", &sys.next_disease_event_position.x);
  ImGui::SliderFloat("NextDiseaseEventR", &sys.next_disease_event_radius, 1.0f, 64.0f);
  ImGui::Checkbox("PreferPlaceP", &sys.prefer_place_position);

  if (ImGui::Button("TriggerEvent")) {
    DiseaseEvent evt{};
    evt.position = sys.next_disease_event_position;
    evt.radius = sys.next_disease_event_radius;
    sys.pending_disease_event = evt;
  }

  ImGui::End();
}

GROVE_NAMESPACE_END
