#include "environment_instruments.hpp"
#include "../audio_core/node_placement.hpp"
#include "../audio_core/rhythm_parameters.hpp"
#include "../audio_core/pitch_sampling.hpp"
#include "../audio_core/AudioComponent.hpp"
#include "../weather/common.hpp"
#include "../audio_processors/ChimeSampler.hpp"
#include "../audio_processors/MultiComponentSampler.hpp"
#include "../audio_processors/Skittering1.hpp"
#include "../audio_processors/TransientsSampler1.hpp"
#include "grove/math/random.hpp"
#include "grove/math/util.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateInfo = env::EnvironmentInstrumentUpdateInfo;

struct EnvironmentInstruments {
  bool initialized{};
  AudioNodeStorage::NodeID ms_node{};
  AudioNodeStorage::NodeID chime_node{};
  AudioNodeStorage::NodeID skittering_node{};
  AudioNodeStorage::NodeID transient_sampler_node{};
  Stopwatch ms_sampler_state_timer;
  Stopwatch chime_muted_state_timer;
  Stopwatch skittering_muted_state_timer;
  Stopwatch transient_sampler_state_timer;
  float chime_muted_state_time{60.0f};
  bool chime_muted{};
  bool chime_started{};
  bool skittering_muted{};
  bool skittering_started{};
  float skittering_muted_state_time{80.0f};
  bool transient_sampler_started{};
  uint8_t transient_sampler_state{};
  int ms_state{};
  int chime_duration_set_index{};
};

bool can_initialize(const AudioComponent& component) {
  const char* buff_names[9] = {
    "piano-c.wav", "flute-c2.wav", "operator-c.wav", "choir-c.wav", "csv-pad.wav",
    "whitney_bird.wav", "chime_c3.wav", "chime2_c3.wav", "cajon.wav"
  };
  for (const char* name : buff_names) {
    auto buff = component.audio_buffers.find_by_name(name);
    if (!buff) {
      return false;
    }
  }
  return true;
}

uint32_t create_transient_sampler(const UpdateInfo& info) {
  auto* audio_component = &info.audio_component;
  auto node_ctor = [audio_component](AudioNodeStorage::NodeID node_id) {
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* transport = &audio_component->audio_transport;

    const uint32_t onsets[32]{
      15771, 34993, 44238, 54877, 68088, 74690, 83120, 94410, 102192, 107237, 114149,
      121055, 132979, 140573, 151761, 160537, 179416, 184906, 190785, 198069, 203866,
      209691, 217366, 228128, 236387, 247071, 265784, 274678, 304375, 312438, 336740,
      342887
    };

    auto buff = buffers->find_by_name("cajon.wav");
    auto buff_handle = buff ? buff.value() : AudioBufferHandle{};
    return new TransientsSampler1(node_id, transport, buff_store, buff_handle, onsets, 32);
  };

  return audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
}

uint32_t create_skittering1(const UpdateInfo& info) {
  auto* audio_component = &info.audio_component;
  auto pss_group = info.pitch_sample_params.get_secondary_group_handle(
    audio_component->get_pitch_sampling_system());

  auto node_ctor = [audio_component, pss_group](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* transport = &audio_component->audio_transport;
    auto* param_sys = audio_component->get_parameter_system();

    AudioBufferHandle buff_handle{};
    if (auto buff = buffers->find_by_name("vocal_unison.wav")) {
      buff_handle = buff.value();
    }

    return new Skittering1(
      node_id, buff_store, transport, scale, param_sys, pss_group.id, buff_handle);
  };

  return audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
}

uint32_t create_multi_component_sampler(const UpdateInfo& info) {
  auto* audio_component = &info.audio_component;

  auto pss_group = info.pitch_sample_params.get_secondary_group_handle(
    audio_component->get_pitch_sampling_system());

  auto node_ctor = [audio_component, pss_group](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* param_sys = audio_component->get_parameter_system();
    auto* transport = &audio_component->audio_transport;

    const char* names[5] = {
      "piano-c.wav",
      "flute-c2.wav",
      "operator-c.wav",
      "choir-c.wav",
      "csv-pad.wav"
    };
    AudioBufferHandle buff_handles[5]{};
    int num_handles{};
    for (int i = 0; i < 5; i++) {
      if (auto handle = buffers->find_by_name(names[i])) {
        buff_handles[num_handles++] = handle.value();
      }
    }
    return new MultiComponentSampler(
      node_id, buff_store, buff_handles, num_handles, scale, transport, param_sys, pss_group.id);
  };

  return audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
}

uint32_t create_chime_sampler(const UpdateInfo& info) {
  auto* audio_component = &info.audio_component;

  auto pss_group = info.pitch_sample_params.get_secondary_group_handle(
    audio_component->get_pitch_sampling_system());

  auto node_ctor = [audio_component, pss_group](AudioNodeStorage::NodeID node_id) {
    auto* scale = audio_component->get_scale();
    auto* buff_store = audio_component->get_audio_buffer_store();
    auto* buffers = &audio_component->audio_buffers;
    auto* transport = &audio_component->audio_transport;
    auto* param_sys = audio_component->get_parameter_system();

    AudioBufferHandle bg_buff_handle{};
    if (auto buff = buffers->find_by_name("whitney_bird.wav")) {
      bg_buff_handle = buff.value();
    }

    AudioBufferHandle buff_handles[4]{};
    int num_handles{};
    const char* buff_names[4] = {"chime_c3.wav", "chime2_c3.wav", "piano-c.wav", "flute-c2.wav"};
    for (const char* name : buff_names) {
      if (auto buff = buffers->find_by_name(name)) {
        buff_handles[num_handles++] = buff.value();
      }
    }

    return new ChimeSampler(
      node_id, buff_store, scale, transport, param_sys, pss_group.id,
      bg_buff_handle, buff_handles, num_handles);
  };

  return audio_component->audio_node_storage.create_node(
    node_ctor, make_port_descriptors_from_audio_node_ctor(node_ctor));
}

bool all_connected(AudioNodeStorage::NodeID node, const UpdateInfo& info) {
  return info.audio_component.audio_node_storage.all_non_optional_ports_connected(node);
}

void update_ms_node(EnvironmentInstruments& insts, const UpdateInfo& info) {
  using WS = weather::State;

  auto& weather = info.weather_status;
  float p_sin{};
  float p_mask_out{};
  float gran_dur{};
  Vec2f p_sin_lims{0.5f, 0.75f};
  Vec2f p_mask_out_lims{0.5f, 0.75f};
  Vec2f gran_dur_lims{0.5f, 0.75f};

  Optional<float> lerp_f;
  if (weather.current == WS::Sunny && weather.next == WS::Overcast) {
    lerp_f = weather.frac_next;

  } else if (weather.current == WS::Overcast && weather.next == WS::Sunny) {
    lerp_f = 1.0f - weather.frac_next;
  }

  if (lerp_f) {
    p_sin = lerp(lerp_f.value(), p_sin_lims.x, p_sin_lims.y);
    p_mask_out = lerp(lerp_f.value(), p_mask_out_lims.x, p_mask_out_lims.y);
    gran_dur = lerp(lerp_f.value(), gran_dur_lims.x, gran_dur_lims.y);
  }

  auto* set_params = info.audio_component.get_simple_set_parameter_system();
  param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "p_sin", p_sin);
  param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "p_masked_out", p_mask_out);
  param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "granule_dur", gran_dur);
  param_system::ui_set_float_value_from_fraction(
    set_params, insts.ms_node, "p_quantized_granule_dur", info.rhythm_params.global_p_quantized);

#if 0
  double state_t = insts.ms_sampler_state_timer.delta().count();
  switch (insts.ms_state) {
    case 0: {
      param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "granule_dur", 0.0f);
      param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "voice_delay_mix", 0.0f);
      if (state_t >= 8.0) {
        insts.ms_state = 1;
        insts.ms_sampler_state_timer.reset();
      }
      break;
    }
    case 1: {
      param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "granule_dur", 0.5f);
      param_system::ui_set_float_value_from_fraction(set_params, insts.ms_node, "voice_delay_mix", 0.5f);
      if (state_t >= 8.0) {
        insts.ms_state = 0;
        insts.ms_sampler_state_timer.reset();
      }
    }
  }
#endif
}

void update_chime_node(EnvironmentInstruments& insts, const UpdateInfo& info) {
  using WS = weather::State;

  auto& weather = info.weather_status;

  if (weather.current == WS::Sunny) {
    insts.chime_duration_set_index = 0;

  } else if (weather.current == WS::Overcast) {
    insts.chime_duration_set_index = 3;
  }

  auto* set_params = info.audio_component.get_simple_set_parameter_system();
  param_system::ui_set_int_value(
    set_params, insts.chime_node, "duration_index", insts.chime_duration_set_index);
  param_system::ui_set_float_value_from_fraction(
    set_params, insts.chime_node, "p_quantized", info.rhythm_params.global_p_quantized);

  if (!insts.chime_started && all_connected(insts.chime_node, info)) {
    insts.chime_muted_state_timer.reset();
    insts.chime_started = true;
  }

  if (insts.chime_started) {
    if (insts.chime_muted_state_timer.delta().count() > insts.chime_muted_state_time) {
      insts.chime_muted = !insts.chime_muted;
      insts.chime_muted_state_timer.reset();
      insts.chime_muted_state_time = lerp(float(urand()), 60.0f, 90.0f);
    }
  }

  param_system::ui_set_float_value_from_fraction(
    set_params, insts.chime_node, "chime_mix", insts.chime_muted ? 0.0f : 1.0f);
}

void update_skittering_node(EnvironmentInstruments& insts, const UpdateInfo& info) {
  auto* set_params = info.audio_component.get_simple_set_parameter_system();
  param_system::ui_set_int_value(set_params, insts.skittering_node, "prefer_midi_input", 1);

  if (!insts.skittering_started && all_connected(insts.skittering_node, info)) {
    insts.skittering_muted_state_timer.reset();
    insts.skittering_started = true;
  }

  if (insts.skittering_started) {
    if (insts.skittering_muted_state_timer.delta().count() > insts.skittering_muted_state_time) {
      insts.skittering_muted = !insts.skittering_muted;
      insts.skittering_muted_state_timer.reset();
      insts.skittering_muted_state_time = lerp(float(urand()), 60.0f, 90.0f);
    }
  }

  param_system::ui_set_float_value_from_fraction(
    set_params, insts.skittering_node, "overall_gain", insts.skittering_muted ? 0.0f : 1.0f);
}

void update_transient_sampler_node(EnvironmentInstruments& insts, const UpdateInfo& info) {
  auto* set_params = info.audio_component.get_simple_set_parameter_system();
  const float p_local_quantized = lerp(info.rhythm_params.global_p_quantized, 0.97f, 1.0f);
  param_system::ui_set_float_value_from_fraction(
    set_params, insts.transient_sampler_node, "p_local_quantized", p_local_quantized);

  if (!insts.transient_sampler_started && all_connected(insts.transient_sampler_node, info)) {
    insts.transient_sampler_state_timer.reset();
    insts.transient_sampler_started = true;
  }

  if (insts.transient_sampler_started) {
    if (insts.transient_sampler_state_timer.delta().count() > 60.0f) {
      insts.transient_sampler_state = uint8_t(!insts.transient_sampler_state);
      insts.transient_sampler_state_timer.reset();
    }
  }

  const float local_time = insts.transient_sampler_state == 0 ? 0.0f : 0.1f;
  param_system::ui_set_float_value_from_fraction(
    set_params, insts.transient_sampler_node, "local_time", local_time);
}

void update_environment_instruments(EnvironmentInstruments& insts, const UpdateInfo& info) {
  if (!insts.initialized && can_initialize(info.audio_component)) {
    insts.initialized = true;

    insts.chime_node = create_chime_sampler(info);
    insts.ms_node = create_multi_component_sampler(info);
    insts.skittering_node = create_skittering1(info);
    insts.transient_sampler_node = create_transient_sampler(info);

    PlaceAudioNodeInWorldParams place_params{};
    place_params.y_offset = 2.0f;
    place_params.terrain = &info.terrain;
    place_params.orientation = SimpleAudioNodePlacement::NodeOrientation::Vertical;

    const uint32_t nodes[4]{
      insts.chime_node, insts.ms_node, insts.skittering_node, insts.transient_sampler_node};

    int ni{};
    for (uint32_t node : nodes) {
      place_audio_node_in_world(
        node, Vec3f{8.0f + float(ni++) * 4.0f, 0.0f, 0.0f},
        info.audio_component.audio_node_storage,
        info.port_placement, info.node_placement, place_params);
    }
  }

  if (!insts.initialized) {
    return;
  }

  update_ms_node(insts, info);
  update_chime_node(insts, info);
  update_skittering_node(insts, info);
  update_transient_sampler_node(insts, info);
}

struct {
  EnvironmentInstruments instruments;
} globals;

} //  anon

void env::update_environment_instruments(const UpdateInfo& info) {
  update_environment_instruments(globals.instruments, info);
}

GROVE_NAMESPACE_END
