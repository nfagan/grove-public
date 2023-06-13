#include "roots_instrument.hpp"
#include "roots_system.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "../audio_core/node_placement.hpp"
#include "../audio_core/pitch_sampling.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../terrain/terrain.hpp"
#include "../audio_processors/SpectrumNode.hpp"
#include "../audio_processors/GaussDistributedPitches1.hpp"
#include "grove/audio/AudioRenderBufferSystem.hpp"
#include "grove/audio/dft.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/History.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

using namespace tree;

namespace {

struct {
  AudioNodeStorage::NodeID spectrum_node{};
  bool initialized_spectrum{};

  AudioNodeStorage::NodeID branch_spawn_node{};
  int next_voice_index{};

  History<Vec2f, 16> xz_spawn_p_history{};
  float xz_spawn_p_var{};
} globals;

void gather_floats(const audio_buffer_system::BufferView& buff, float* samples, int num_frames) {
  const unsigned char* data = buff.data_ptr();
  for (int i = 0; i < num_frames; i++) {
    auto data0 = data + i * sizeof(float) * 2;
    auto data1 = data + i * sizeof(float) * 2 + sizeof(float);
    memcpy(samples + i * 2, data0, sizeof(float));
    memcpy(samples + i * 2 + 1, data1, sizeof(float));
  }
}

AudioNodeStorage::NodeID create_branch_spawn_node(const RootsInstrumentContext& context) {
  auto* audio_component = &context.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* param_sys = audio_component->get_parameter_system();
    return new GaussDistributedPitches1(node_id, scale, param_sys);
  };
  return audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
}

} //  anon

RootsInstrumentUpdateResult
tree::update_roots_spectrum_growth_instrument(const RootsInstrumentContext& context) {
  //
  RootsInstrumentUpdateResult result;

  if (!globals.initialized_spectrum) {
    auto& node_storage = context.audio_component.audio_node_storage;
    auto node_ctor = [](AudioNodeStorage::NodeID node_id) { return new SpectrumNode(node_id); };
    globals.spectrum_node = node_storage.create_node(
      node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
    node_storage.construct_instance(globals.spectrum_node);

    Vec3f pos{0.0f, 6.0f, 0.0f};
    pos.y += context.terrain.height_nearest_position_xz(pos);

    auto port_info = node_storage.get_port_info_for_node(globals.spectrum_node).unwrap();
    auto place_res = context.node_placement.create_node(
      globals.spectrum_node, port_info, pos, 6.0f,
      SimpleAudioNodePlacement::NodeOrientation::Horizontal);
    for (auto& info : place_res) {
      context.port_placement.add_selectable_with_bounds(info.id, info.world_bound);
    }

    globals.initialized_spectrum = true;
  }

  const uint32_t node_id = globals.spectrum_node;
  const auto view_buffs = audio_buffer_system::ui_read_newly_received();
  auto buff_it = std::find_if(view_buffs.begin(), view_buffs.end(), [&](auto& rcv) {
    //  @TODO: Codify that type tag == 1 means spectrum.
    return rcv.type_tag == 1 && rcv.instance_id == node_id && rcv.buff.is_float2();
  });

  if (buff_it != view_buffs.end()) {
    const auto num_frames = int(buff_it->buff.num_frames());

    Temporary<float, 1024> store_floats;
    float* samples = store_floats.require(num_frames * 2);
    gather_floats(buff_it->buff, samples, num_frames);

    Temporary<float, 1024> store_magnitudes;
    auto* magnitudes = store_magnitudes.require(num_frames);
    complex_moduli(samples, magnitudes, num_frames);

    for (int i = 0; i < num_frames; i++) {
      magnitudes[i] = float(amplitude_to_db(magnitudes[i]));
    }

    const int ns = num_frames / 2;
    float sum_mag = std::accumulate(magnitudes, magnitudes + ns, 0.0f);
    sum_mag /= float(ns);

    float lerp_t{};
    if (std::isfinite(sum_mag)) {
      const float lims[2] = {float(minimum_finite_gain()), -10.0f};
      lerp_t = (clamp(sum_mag, lims[0], lims[1]) - lims[0]) / (lims[1] - lims[0]);
    }

    result.new_spectral_fraction = lerp_t;
  }

  return result;
}

void tree::update_roots_branch_spawn_instrument(
  const RootsInstrumentContext& context, const RootsNewBranchInfo* infos, int num_infos) {
  //
  if (!globals.branch_spawn_node) {
    globals.branch_spawn_node = create_branch_spawn_node(context);

    PlaceAudioNodeInWorldParams place_params{};
    place_params.y_offset = 2.0f;
    place_params.terrain = &context.terrain;
    place_params.orientation = SimpleAudioNodePlacement::NodeOrientation::Vertical;

    place_audio_node_in_world(
      globals.branch_spawn_node, Vec3f{8.0f, 0.0f, 8.0f},
      context.audio_component.audio_node_storage,
      context.port_placement, context.node_placement, place_params);
  }

#if 1
  auto* pss = context.audio_component.get_pitch_sampling_system();
  PitchClass pcs[12];
  pcs[0] = PitchClass::C;
  int num_pcs = std::max(1, pss::ui_read_unique_pitch_classes_in_sample_set(
    pss, context.pitch_sampling_params.get_secondary_group_handle(pss), 0, pcs));
#endif

  const char* mu_params[4]{"mu0", "mu1", "mu2", "mu3"};
  const char* sigma_params[4]{"sigma0", "sigma1", "sigma2", "sigma3"};

  const float min_y = -96.0f;
  const float max_y = 96.0f;

  const int min_mu = GaussDistributedPitches1::min_mu;
  const int max_mu = GaussDistributedPitches1::max_mu;
  int oct_span = std::max(1, (max_mu - min_mu) / 12);

  const float min_var = 20.0f;
  const float max_var = 512.0f;
  const float max_sigma = std::min(GaussDistributedPitches1::max_sigma, 0.5f);
  const float frac_max_sigma = max_sigma / GaussDistributedPitches1::max_sigma;

  for (int i = 0; i < num_infos; i++) {
    const Vec3f pos = infos[i].position;

    float sigma_t;
    {
      globals.xz_spawn_p_history.push(Vec2f{pos.x, pos.z});
      auto new_var_xz = globals.xz_spawn_p_history.var_or_default(Vec2f{});
      float new_var = std::max(new_var_xz.x, new_var_xz.y);
      globals.xz_spawn_p_var = lerp(0.75f, globals.xz_spawn_p_var, new_var);
      float var_t = inv_lerp_clamped(globals.xz_spawn_p_var, min_var, max_var);
      sigma_t = std::pow(var_t, 2.0f) * frac_max_sigma;
    }

    const float ty = inv_lerp_clamped(pos.y, min_y, max_y);
    int mu = int(lerp(ty, float(min_mu), float(max_mu)));
    int oct = int(float(oct_span) * ty - float(oct_span) * 0.5f + 3);
    mu = clamp(int(pcs[wrap_within_range(mu, num_pcs)]) + oct * 12 - 36, min_mu, max_mu);

    const int num_lobes = std::min(GaussDistributedPitches1::num_lobes, 4);
    const int li = globals.next_voice_index;
    globals.next_voice_index = (globals.next_voice_index + 1) % num_lobes;

    auto* set_params = context.audio_component.get_simple_set_parameter_system();
    param_system::ui_set_int_value(set_params, globals.branch_spawn_node, mu_params[li], mu);
    param_system::ui_set_float_value_from_fraction(
      set_params, globals.branch_spawn_node, sigma_params[li], sigma_t);
  }
}

GROVE_NAMESPACE_END
