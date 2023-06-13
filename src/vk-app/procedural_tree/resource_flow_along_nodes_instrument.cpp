#include "resource_flow_along_nodes_instrument.hpp"
#include "resource_flow_along_nodes.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "../audio_core/audio_port_placement.hpp"
#include "../audio_processors/SteerableSynth1.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../terrain/terrain.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/common.hpp"
#include "grove/audio/envelope.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateResult = tree::ResourceFlowAlongNodesInstrumentUpdateResult;

struct {
  bool initialized{};
  bool inserted_node_bounds_into_accel{};
  AudioNodeStorage::NodeID node_id{};
  AudioParameterWriterID param_writer_id{};
  Optional<AudioParameterDescriptor> pitch_bend;
  Optional<AudioParameterDescriptor> reverb_mix;
  Optional<AudioParameterDescriptor> noise_gain;
  Stopwatch pb_state_timer;
  int pb_state{};
  Stopwatch rev_state_timer;
  int rev_state{};
  audio::ExpInterpolated<float> rev_mix_frac{0.0f};

} globals;

} //  anon

UpdateResult tree::update_resource_flow_along_nodes_instrument(
  ResourceSpiralAroundNodesSystem* sys, AudioComponent& audio_component,
  SimpleAudioNodePlacement& node_placement, AudioPortPlacement& port_placement,
  const PitchSampleSetGroupHandle& pitch_sample_group, const Terrain& terrain, double real_dt) {
  //
  UpdateResult result{};

  if (!globals.initialized) {
    auto node_ctor = [component = &audio_component, pitch_sample_group](AudioNodeStorage::NodeID node_id) {
      auto* scale = component->get_scale();
      auto* param_sys = component->get_parameter_system();
      return new SteerableSynth1(node_id, param_sys, scale, pitch_sample_group.id);
    };

    globals.node_id = audio_component.audio_node_storage.create_node(
      node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
    audio_component.audio_node_storage.construct_instance(globals.node_id);

    {
      auto* param_sys = audio_component.get_parameter_system();
      auto* write = param_system::ui_get_write_access(param_sys);
      globals.param_writer_id = AudioParameterWriteAccess::create_writer();

      Temporary<AudioParameterDescriptor, 256> store_param_descs;
      auto view_stack = store_param_descs.view_stack();
      audio_component.audio_node_storage.audio_parameter_descriptors(globals.node_id, view_stack);
      for (auto& p : view_stack) {
        if (p.matches_name("pitch_bend")) {
          if (write->request(globals.param_writer_id, p)) {
            globals.pitch_bend = p;
          }
        } else if (p.matches_name("reverb_mix")) {
          if (write->request(globals.param_writer_id, p)) {
            globals.reverb_mix = p;
          }
        } else if (p.matches_name("noise_gain")) {
          if (write->request(globals.param_writer_id, p)) {
            globals.noise_gain = p;
          }
        }
      }
    }

    Vec3f pos{0.0f, 0.0f, 0.0f};
    pos.y = terrain.height_nearest_position_xz(pos) + 2.0f;

    auto port_info = audio_component.audio_node_storage.get_port_info_for_node(globals.node_id).unwrap();
    auto place_res = node_placement.create_node(
      globals.node_id, port_info, pos, 2.0f, SimpleAudioNodePlacement::NodeOrientation::Horizontal);
    for (auto& info : place_res) {
      port_placement.add_selectable_with_bounds(info.id, info.world_bound);
    }

    globals.pb_state_timer.reset();
    globals.initialized = true;
  }

  if (globals.initialized && !globals.inserted_node_bounds_into_accel) {
    result.insert_node_bounds_into_accel = node_placement.get_node_bounds(
      globals.node_id, audio_component.audio_node_storage, terrain);
    result.acknowledge_inserted = &globals.inserted_node_bounds_into_accel;
  }

  auto pb_state_time = globals.pb_state_timer.delta().count();
  if (globals.pb_state == 0) {
    if (pb_state_time > 64.0) {
      globals.pb_state = 1;
      globals.pb_state_timer.reset();
    }
  } else {
    if (pb_state_time > 64.0) {
      globals.pb_state = 0;
      globals.pb_state_timer.reset();
    }
  }

  auto rev_state_time = globals.rev_state_timer.delta().count();
  if (globals.rev_state == 0) {
    if (rev_state_time > 64.0) {
      globals.rev_state = 1;
      globals.rev_state_timer.reset();
    }
  } else {
    if (rev_state_time > 32.0) {
      globals.rev_state = 0;
      globals.rev_state_timer.reset();
    }
  }

  auto* param_sys = audio_component.get_parameter_system();
  if (globals.pitch_bend) {
    float t{};
    float th{pif() * 0.25f};

    if (globals.pb_state == 0) {
      t = 0.5f;
    } else if (globals.pb_state == 1) {
      t = 0.0f;
      th = -pif() * 0.5f + 0.1f;
    }

    auto& p = globals.pitch_bend.value();
    auto val = make_interpolated_parameter_value_from_descriptor(p, t);
    param_system::ui_set_value(param_sys, globals.param_writer_id, p.ids, val);

    tree::set_global_theta(sys, 0, th);
  }

  if (globals.reverb_mix && globals.noise_gain) {
    globals.rev_mix_frac.set_time_constant95(4.0f);
    globals.rev_mix_frac.set_target(globals.rev_state == 1 ? 1.0f : 0.0f);

    float rev_frac = globals.rev_mix_frac.tick(float(1.0 / std::max(1e-5, real_dt)));
    float rev_t = lerp(rev_frac, 0.0f, 0.75f);
    float noise_t = lerp(rev_frac, 0.125f, 0.5f);
    float vel = lerp(rev_frac, 6.0f, 2.0f);

    auto& rev_p = globals.reverb_mix.value();
    auto& noise_p = globals.noise_gain.value();
    auto rev_val = make_interpolated_parameter_value_from_descriptor(rev_p, rev_t);
    auto noise_val = make_interpolated_parameter_value_from_descriptor(noise_p, noise_t);
    param_system::ui_set_value(param_sys, globals.param_writer_id, rev_p.ids, rev_val);
    param_system::ui_set_value(param_sys, globals.param_writer_id, noise_p.ids, noise_val);

    tree::set_global_velocity_scale(sys, 0, vel);
  }

  return result;
}

GROVE_NAMESPACE_END
