#include "AudioNodeIsolator.hpp"
#include "AudioRenderable.hpp"
#include "./types.hpp"
#include "./data_channel.hpp"
#include "grove/math/ease.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/Handshake.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

struct Target {
  uint32_t node;
  bool input;

  friend bool operator==(const Target& a, const Target& b) {
    return a.node == b.node && a.input == b.input;
  }

  friend bool operator!=(const Target& a, const Target& b) {
    return !(a == b);
  }
};

struct CapturedBufferData {
  void require(const AudioRenderInfo& info) {
    samples.resize(info.num_frames * info.num_channels);
  }

  void zero() {
    std::fill(samples.begin(), samples.end(), 0.0f);
  }

  std::vector<float> samples;
};

struct CapturedBufferState {
public:
  enum class State {
    Inactive = 0,
    Transitioning,
    Active,
  };

public:
  float increment_gain(float gi) {
    assert(gi > 0.0f);
    if (gain_target_high) {
      gain = clamp01(gain + gi);
    } else {
      gain = clamp01(gain - gi);
    }
    return ease::in_out_expo(gain);
  }

  float gain_target_value() const {
    return gain_target_high ? 1.0f : 0.0f;
  }

  bool reached_target() const {
    return gain == gain_target_value();
  }

  bool active() const {
    return state == State::Active;
  }

  bool inactive() const {
    return state == State::Inactive;
  }

  bool transitioning() const {
    return state == State::Transitioning;
  }

  bool not_inactive() const {
    return active() || transitioning();
  }

public:
  State state{};
  float gain{};
  bool gain_target_high{};
  Target node_target{};
};

struct Modification {
  Target target;
  bool deactivate;
};

struct RenderData {
  const AudioRenderable* target{};
  CapturedBufferState primary{};
  CapturedBufferState auxiliary{};
  CapturedBufferData primary_data{};
  CapturedBufferData auxiliary_data{};
  bool transitioning{};
  int num_channels_reserved{};
  int num_frames_reserved{};
  float solo_gain{1.0f};
};

struct UIData {
  Optional<Modification> pending;
  Optional<Target> active;

  Optional<uint32_t> isolating_inputs_node_id;
  Optional<uint32_t> isolating_outputs_node_id;
};

struct AudioNodeIsolator {
  std::atomic<const AudioRenderable*> canonical_target_renderable{};
  std::atomic<float> canonical_solo_gain{};

  UIData ui;
  RenderData render;

  Handshake<Modification> modification{};
  std::atomic<bool> finished_transition{};
  bool awaiting_finish_transition{};
  Optional<Target> pending_finish_activate;
  Optional<Target> pending_finish_deactivate;
};

namespace {

bool check_finished_transition(CapturedBufferState& buff) {
  if (!buff.reached_target()) {
    return false;
  }

  if (buff.gain_target_high) {
    buff.state = CapturedBufferState::State::Active;
  } else {
    buff = {};
  }

  return true;
}

bool want_isolate(const CapturedBufferState& state, uint32_t node, bool in) {
  return state.not_inactive() && state.node_target.node == node && state.node_target.input == in;
}

bool want_isolate_input(const CapturedBufferState& state, uint32_t node) {
  return want_isolate(state, node, true);
}

bool want_isolate_output(const CapturedBufferState& state, uint32_t node) {
  return want_isolate(state, node, false);
}

void render_isolate(
  AudioNodeIsolator* isolator, bool input, uint32_t node, const unsigned char* in_data,
  const BufferChannelDescriptor* channel_descs, int num_channels, int num_frames) {
  //
  auto& render = isolator->render;
  assert(render.num_frames_reserved == num_frames);
  //  `num_channels` is the number of `channel_descs`, which might be different from the number
  //  of channels reserved by the isolator.
  if (num_channels == 0) {
    return;
  }

  Temporary<int, 32> store_src_channel_desc_indices;
  int* src_channel_desc_indices = store_src_channel_desc_indices.require(num_channels);
  int num_src_float_descs{};

  for (int i = 0; i < num_channels; i++) {
    if (channel_descs[i].is_float()) {
      src_channel_desc_indices[num_src_float_descs++] = i;
    }
  }

  if (num_src_float_descs == 0) {
    //  no acceptable float channels
    return;
  }

  CapturedBufferData* target_data{};
  if (want_isolate(render.primary, node, input)) {
    assert(!want_isolate(render.auxiliary, node, input));
    target_data = &render.primary_data;
  } else {
    assert(want_isolate(render.auxiliary, node, input));
    target_data = &render.auxiliary_data;
  }

  for (int i = 0; i < render.num_channels_reserved; i++) {
    //  @NOTE: This duplicates (in a round-robin fashion) one or more source channels when there are
    //  fewer source channels than destination channels. This also only uses the first N source
    //  channels when there are more of them than destination channels.
    const int srci = i % num_src_float_descs;
    const auto& src_desc = channel_descs[src_channel_desc_indices[srci]];
    assert(src_desc.is_float() && src_desc.size() == sizeof(float));

    for (uint32_t j = 0; j < uint32_t(num_frames); j++) {
      //  destination data is a ptr to float
      const uint32_t di = uint32_t(render.num_channels_reserved) * j + uint32_t(i);

      //  destination buffer should be cleared in begin_render and only written-to once.
      assert(target_data->samples[di] == 0.0f);

      const auto* src = in_data + src_desc.ptr_offset(int64_t(j));
      memcpy(&target_data->samples[di], src, sizeof(float));
    }
  }
}

void ui_start_isolating(AudioNodeIsolator* isolator, uint32_t node, bool input) {
  Target targ{};
  targ.node = node;
  targ.input = input;

  if ((isolator->ui.active && isolator->ui.active.value() == targ) ||
      (isolator->pending_finish_activate && isolator->pending_finish_activate.value() == targ)) {
    //  Already active or awaiting activation.
    return;
  }

  Modification mod{};
  mod.target = targ;
  isolator->ui.pending = mod;
}

void ui_stop_isolating(AudioNodeIsolator* isolator, uint32_t node, bool input) {
  Target targ{};
  targ.node = node;
  targ.input = input;

  if (isolator->pending_finish_deactivate && isolator->pending_finish_deactivate.value() == targ) {
    //  already awaiting finished deactivation
    return;

  } else if (isolator->ui.active && isolator->ui.active.value() == targ) {
    //  common case - stop isolating the input / output that is actively being isolated.
    Modification mod{};
    mod.target = targ;
    mod.deactivate = true;
    isolator->ui.pending = mod;

  } else if (isolator->ui.pending) {
    //  less common case - we intended to start isolating this target but never actually started
    //  actively isolating it. in that case, just clear the pending modification to avoid
    //  activating the target.
    auto& pend = isolator->ui.pending;
    if (pend.value().target == targ && !pend.value().deactivate) {
      pend = NullOpt{};
    }
  }
}

struct {
  AudioNodeIsolator isolator;
} globals;

} //  anon

AudioNodeIsolator* ni::get_global_audio_node_isolator() {
  return &globals.isolator;
}

void ni::ui_init_audio_node_isolator(
  AudioNodeIsolator* isolator, const AudioRenderable* target_renderer) {
  //
  isolator->canonical_solo_gain.store(0.25f);
  isolator->canonical_target_renderable.store(target_renderer);
}

void ni::ui_set_solo_gain(AudioNodeIsolator* isolator, float g) {
  assert(g >= 0.0f);
  isolator->canonical_solo_gain.store(g);
}

bool ni::render_want_isolate_input(const AudioNodeIsolator* isolator, uint32_t node) {
  return want_isolate_input(isolator->render.primary, node) ||
         want_isolate_input(isolator->render.auxiliary, node);
}

void ni::render_isolate_input(
  AudioNodeIsolator* isolator, uint32_t node,
  const AudioProcessData& pd, const AudioRenderInfo& info) {
  //
  render_isolate_input(
    isolator, node, pd.buffer.data,
    pd.descriptors.data(), int(pd.descriptors.size()), info.num_frames);
}

void ni::render_isolate_input(
  AudioNodeIsolator* isolator, uint32_t node, const unsigned char* in_data,
  const BufferChannelDescriptor* channel_descs, int num_channels, int num_frames) {
  //
  assert(render_want_isolate_input(isolator, node));
  render_isolate(isolator, true, node, in_data, channel_descs, num_channels, num_frames);
}

bool ni::render_want_isolate_output(const AudioNodeIsolator* isolator, uint32_t node) {
  return want_isolate_output(isolator->render.primary, node) ||
         want_isolate_output(isolator->render.auxiliary, node);
}

void ni::render_isolate_output(
  AudioNodeIsolator* isolator, uint32_t node,
  const AudioProcessData& pd, const AudioRenderInfo& info) {
  //
  render_isolate_output(
    isolator, node, pd.buffer.data,
    pd.descriptors.data(), int(pd.descriptors.size()), info.num_frames);
}

void ni::render_isolate_output(
  AudioNodeIsolator* isolator, uint32_t node, const unsigned char* in_data,
  const BufferChannelDescriptor* channel_descs, int num_channels, int num_frames) {
  //
  assert(render_want_isolate_output(isolator, node));
  render_isolate(isolator, false, node, in_data, channel_descs, num_channels, num_frames);
}

void ni::begin_render(AudioNodeIsolator* isolator, const AudioRenderInfo& info) {
  auto& render = isolator->render;

  //  Set the target renderable once at the beginning of the block.
  render.target = isolator->canonical_target_renderable.load();

  //  Set the solo-gain
  render.solo_gain = isolator->canonical_solo_gain.load();

  render.primary_data.require(info);
  render.primary_data.zero();

  render.auxiliary_data.require(info);
  render.auxiliary_data.zero();

  render.num_channels_reserved = info.num_channels;
  render.num_frames_reserved = info.num_frames;

  if (auto opt_mod = read(&isolator->modification)) {
    assert(!render.transitioning);
    const Modification& mod = opt_mod.value();

    if (mod.deactivate) {
      assert(render.primary.active() && render.auxiliary.inactive());
      assert(render.primary.node_target == mod.target);

      render.primary.node_target = mod.target;
      render.primary.state = CapturedBufferState::State::Transitioning;
      render.primary.gain_target_high = false;

    } else {
      assert(render.primary.node_target != mod.target);
      assert(render.auxiliary.inactive());

      if (render.primary.active()) {
        render.auxiliary = render.primary;
        render.auxiliary.gain_target_high = false;
        render.auxiliary.state = CapturedBufferState::State::Transitioning;
      }

      render.primary.state = CapturedBufferState::State::Transitioning;
      render.primary.node_target = mod.target;
      render.primary.gain = 0.0f;
      render.primary.gain_target_high = true;
    }

    render.transitioning = true;
  }
}

void ni::process(AudioNodeIsolator* isolator, const AudioRenderable* renderable,
                 float* renderable_generated_samples, const AudioRenderInfo& info) {
  auto& render = isolator->render;

  if (renderable != render.target) {
    return;
  }

  assert(info.num_frames == render.num_frames_reserved);
  assert(info.num_channels == render.num_channels_reserved);

  const float fade_interval_s = 0.125f;
  const auto gain_incr_per_sample = float(1.0f / (info.sample_rate * fade_interval_s));
  assert(gain_incr_per_sample > 0.0f);
  const float solo_g = render.solo_gain;

  if (render.primary.not_inactive() && render.auxiliary.not_inactive()) {
    //  Both transitioning -> cross-fade between them.
    assert(render.primary.gain_target_high && !render.auxiliary.gain_target_high);

    for (int i = 0; i < info.num_frames; i++) {
      const float pg = render.primary.increment_gain(gain_incr_per_sample);
      const float ag = render.auxiliary.increment_gain(gain_incr_per_sample);
      (void) ag;

      for (int j = 0; j < info.num_channels; j++) {
        const int off = i * info.num_channels + j;
        const float aux = solo_g * render.auxiliary_data.samples[off];
        const float prim = solo_g * render.primary_data.samples[off];
        renderable_generated_samples[off] = lerp(pg, aux, prim);
      }
    }

  } else if (render.primary.not_inactive()) {
    //  Only primary, cross-fade between rendered channels and isolated channels
    for (int i = 0; i < info.num_frames; i++) {
      const float pg = render.primary.increment_gain(gain_incr_per_sample);
      for (int j = 0; j < info.num_channels; j++) {
        const int off = i * info.num_channels + j;
        const float prim = solo_g * render.primary_data.samples[off];
        renderable_generated_samples[off] = lerp(pg, renderable_generated_samples[off], prim);
      }
    }
  }
}

void ni::end_render(AudioNodeIsolator* isolator) {
  auto& render = isolator->render;

  if (render.transitioning) {
    assert(render.primary.transitioning() || render.auxiliary.transitioning());

    bool all_reached{true};
    if (render.primary.transitioning()) {
      if (!check_finished_transition(render.primary)) {
        all_reached = false;
      }
    }

    if (render.auxiliary.transitioning()) {
      if (!check_finished_transition(render.auxiliary)) {
        all_reached = false;
      }
    }

    if (all_reached) {
      render.transitioning = false;
      assert(!isolator->finished_transition.load());
      isolator->finished_transition.store(true);
    }
  }
}

void ni::ui_isolate_input(AudioNodeIsolator* isolator, uint32_t node) {
  ui_start_isolating(isolator, node, true);
}

void ni::ui_isolate_output(AudioNodeIsolator* isolator, uint32_t node) {
  ui_start_isolating(isolator, node, false);
}

void ni::ui_stop_isolating_input(AudioNodeIsolator* isolator, uint32_t node) {
  ui_stop_isolating(isolator, node, true);
}

void ni::ui_stop_isolating_output(AudioNodeIsolator* isolator, uint32_t node) {
  ui_stop_isolating(isolator, node, false);
}

bool ni::ui_is_isolating(const AudioNodeIsolator* isolator, uint32_t node, bool input) {
  const auto& check = input ?
    isolator->ui.isolating_inputs_node_id :
    isolator->ui.isolating_outputs_node_id;
  return check && check.value() == node;
}

void ni::ui_toggle_isolating(AudioNodeIsolator* isolator, uint32_t node, bool input) {
  auto& ui = isolator->ui;
  if (input) {
    if (ui.isolating_inputs_node_id && ui.isolating_inputs_node_id.value() == node) {
      ni::ui_stop_isolating_input(isolator, node);
      ui.isolating_inputs_node_id = NullOpt{};
    } else {
      ni::ui_isolate_input(isolator, node);
      ui.isolating_inputs_node_id = node;
      ui.isolating_outputs_node_id = NullOpt{};
    }
  } else {
    if (ui.isolating_outputs_node_id && ui.isolating_outputs_node_id.value() == node) {
      ni::ui_stop_isolating_output(isolator, node);
      ui.isolating_outputs_node_id = NullOpt{};
    } else {
      ni::ui_isolate_output(isolator, node);
      ui.isolating_outputs_node_id = node;
      ui.isolating_inputs_node_id = NullOpt{};
    }
  }
}

ni::AudioNodeIsolatorUpdateResult ni::ui_update(AudioNodeIsolator* isolator) {
  ni::AudioNodeIsolatorUpdateResult result{};

  auto& ui = isolator->ui;

  //  Only one node (output or input) should be isolated at once, or else neither should be.
  assert((!ui.isolating_inputs_node_id && !ui.isolating_outputs_node_id) ||
         ui.isolating_inputs_node_id.has_value() != ui.isolating_outputs_node_id.has_value());

  if (isolator->modification.awaiting_read && acknowledged(&isolator->modification)) {
    assert(!isolator->awaiting_finish_transition);
    isolator->awaiting_finish_transition = true;
  }

  if (isolator->awaiting_finish_transition && isolator->finished_transition.load()) {
    //  Render has finished applying the last change.
    isolator->awaiting_finish_transition = false;
    isolator->finished_transition.store(false);

    //  Update state of active target.
    if (isolator->pending_finish_activate) {
      assert(!ui.active || ui.active.value() != isolator->pending_finish_activate.value());
      ui.active = isolator->pending_finish_activate;
      isolator->pending_finish_activate = NullOpt{};
    } else {
      assert(ui.active && isolator->pending_finish_deactivate);
      ui.active = NullOpt{};
      isolator->pending_finish_deactivate = NullOpt{};
    }
  }

  if (ui.pending && !isolator->modification.awaiting_read &&
      !isolator->awaiting_finish_transition) {
    //  Submit change to render.
    if (ui.pending.value().deactivate) {
      //  Pending change deactivates the target.
      assert(ui.active && !isolator->pending_finish_deactivate);
      isolator->pending_finish_deactivate = ui.pending.value().target;
      //  Signal pending change.
      result.newly_will_deactivate = ui.pending.value().target.node;
    } else {
      //  Pending change activates the target.
      assert(!isolator->pending_finish_activate);
      assert(!isolator->ui.active || isolator->ui.active.value() != ui.pending.value().target);
      isolator->pending_finish_activate = ui.pending.value().target;
      //  Signal pending changes.
      if (isolator->ui.active) {
        result.newly_will_deactivate = ui.active.value().node;
      }
      result.newly_will_activate = ui.pending.value().target.node;
    }

    publish(&isolator->modification, std::move(ui.pending.value()));
    ui.pending = NullOpt{};
  }

  return result;
}

GROVE_NAMESPACE_END
