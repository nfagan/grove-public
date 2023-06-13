#include "TriggeredBufferRenderer.hpp"
#include "AudioBufferStore.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/vector_util.hpp"
#include "grove/math/util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using Instance = TriggeredBufferRenderer::Instance;
using Instances_ = TriggeredBufferRenderer::Instances_;

struct InstanceHandleLess {
  bool operator()(const Instance& a, const Instance& b) const noexcept {
    return a.instance_handle < b.instance_handle;
  }
  bool operator()(const Instance& a, TriggeredBufferHandle b) const noexcept {
    return a.instance_handle < b;
  }
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "TriggeredBufferRenderer";
}

constexpr float time_to_change_gain_s() {
  return 5e-3f;
}

constexpr float min_gain_abort() {
  return 0.001f;
}

const Instance* find_instance(const Instances_& instances, TriggeredBufferHandle handle) {
  auto it = std::lower_bound(instances.begin(), instances.end(), handle, InstanceHandleLess{});
  if (it != instances.end() && it->instance_handle == handle) {
    return &*it;
  } else {
    return nullptr;
  }
}

inline bool begin_fadeout(double frame_index, uint64_t total_num_frames, double sample_rate) {
  auto frames_remain = total_num_frames - uint64_t(frame_index);
  return double(frames_remain) / sample_rate <= time_to_change_gain_s();
}

void render_instance(const Instance& instance,
                     const AudioBufferStore* buffer_store,
                     Sample* samples,
                     const AudioRenderInfo& info) {
  if (instance.state->expired.load()) {
    return;
  }

  if (instance.state->abort_triggered.load()) {
    //  Fade out.
    instance.state->gain.target = 0.0f;
  }

  auto& frame_index = instance.state->frame_index;
  const auto rate_multiplier = instance.playback_rate_multiplier;
  const auto loop_type = instance.loop_type;

  auto hint_frame_begin = uint64_t(frame_index);
  const auto maybe_chunk = buffer_store->render_get(
    instance.buffer_handle, hint_frame_begin, hint_frame_begin + info.num_frames);

  bool just_elapsed = false;

  if (maybe_chunk && maybe_chunk.value().descriptor.is_n_channel_float(info.num_channels)) {
    const auto& chunk = maybe_chunk.value();

    const auto total_num_frames = chunk.num_frames_in_source();
    const auto src_sr = chunk.descriptor.sample_rate;
    const auto out_sr = info.sample_rate;
    const double isi = 1.0 / out_sr;

    for (int i = 0; i < info.num_frames; i++) {
      instance.state->timeout_s = std::max(0.0f, float(instance.state->timeout_s - isi));
      if (instance.state->timeout_s > 0.0f) {
        continue;
      }

      auto interp_info = util::make_linear_interpolation_info(frame_index, total_num_frames);
      if (!chunk.is_in_bounds(interp_info.i0) || !chunk.is_in_bounds(interp_info.i1)) {
        just_elapsed = true;
        break;
      }

      if (instance.fade_out && begin_fadeout(frame_index, total_num_frames, out_sr)) {
        instance.state->gain.target = 0.0f;
      }

      const auto gain = instance.state->gain.tick(float(info.sample_rate));
      for (int j = 0; j < info.num_channels; j++) {
        auto* out = samples + i * info.num_channels + j;
        auto channel_descriptor = chunk.channel_descriptor(j);
        auto src_val = util::tick_interpolated_float(chunk, channel_descriptor, interp_info);
        *out += src_val * gain;
      }

      if (loop_type == TriggeredBufferLoopType::None) {
        frame_index += frame_index_increment(src_sr, out_sr, rate_multiplier);

      } else if (loop_type == TriggeredBufferLoopType::Forwards) {
        frame_index = util::tick_interpolating_frame_index_forwards_loop(
          frame_index, src_sr, out_sr, rate_multiplier, total_num_frames);

      } else {
        //  Unhandled loop type.
        assert(false);
      }
    }
  } else {
    //  Failed to load chunk, or else the chunk has an incompatible layout.
    just_elapsed = true;
  }

  bool did_expire = just_elapsed;
  if (instance.state->abort_triggered.load() &&
      instance.state->gain.current <= min_gain_abort()) {
    //  Done fading out after abort was triggered.
    did_expire = true;
  }

  if (did_expire) {
    instance.state->expired.store(true);
  }
}

} //  anon

TriggeredBufferRenderer::TriggeredBufferRenderer(const AudioBufferStore* buffer_store) :
  buffer_store{buffer_store} {
  //
}

void TriggeredBufferRenderer::render_apply_modifications(const Instances_& to_render) {
  assert(std::is_sorted(to_render.begin(), to_render.end(), InstanceHandleLess{}));
  int num_pend = pending_modifications.size();
  for (int i = 0; i < num_pend; i++) {
    auto mod = pending_modifications.read();
    if (auto* instance = find_instance(to_render, mod.handle)) {
      if (mod.gain) {
        //  Target towards gain.
        instance->state->gain.target = mod.gain.value();
      }
    } else {
      GROVE_LOG_WARNING_CAPTURE_META("Modification not applied; no such instance.", logging_id());
    }
  }
}

void TriggeredBufferRenderer::render(const AudioRenderer&,
                                     Sample* samples,
                                     AudioEvents*,
                                     const AudioRenderInfo& info) {
  const auto& to_render = instance_accessor.maybe_swap_and_read();
  render_apply_modifications(to_render);

  for (auto& instance : to_render) {
    render_instance(instance, buffer_store, samples, info);
  }
}

UITriggeredBufferInstance
TriggeredBufferRenderer::ui_play(AudioBufferHandle buffer_handle,
                                 const TriggeredBufferPlayParams& params) {
  auto state = std::make_shared<impl::TriggeredBufferSharedState>();
  TriggeredBufferHandle instance_handle{next_instance_id++};

  UITriggeredBufferInstance result{};
  result.handle = instance_handle;
  result.state = state;

  state->gain.target = params.gain;
  state->gain.set_time_constant95(time_to_change_gain_s());
  state->timeout_s = params.timeout_s;

  Instance pending{};
  pending.instance_handle = instance_handle;
  pending.buffer_handle = buffer_handle;
  pending.state = state;
  pending.loop_type = params.loop_type;
  pending.playback_rate_multiplier = params.playback_rate_multiplier;
  pending.fade_out = params.fade_out;
  pending_ui_submit.push_back(pending);

  return result;
}

void TriggeredBufferRenderer::ui_submit_pending_modifications() {
  int num_free = pending_modifications.num_free();
  auto ui_begin = ui_pending_modifications.begin();

  while (num_free > 0 && !ui_pending_modifications.empty()) {
    pending_modifications.write(std::move(ui_begin->second));
    ui_begin = ui_pending_modifications.erase(ui_begin);
    num_free--;
  }

  if (!ui_pending_modifications.empty()) {
    GROVE_LOG_WARNING_CAPTURE_META("Not all ui modifications processed this frame.", logging_id());
  }
}

void TriggeredBufferRenderer::ui_update() {
  ui_submit_pending_modifications();

  if (instance_accessor.writer_can_modify()) {
    Instances_& write_to = *instance_accessor.writer_ptr();
    //  First erase any expired triggered buffers.
    DynamicArray<int, 4> erase_at;
    for (int i = 0; i < int(write_to.size()); i++) {
      auto& curr = write_to[i];
      if (curr.state->expired.load()) {
        erase_at.push_back(i);
      }
    }

    const bool requires_modification = !erase_at.empty() || !pending_ui_submit.empty();
    if (requires_modification) {
      (void) instance_accessor.writer_begin_modification();
      erase_set(write_to, erase_at);
      for (auto& pend : pending_ui_submit) {
        write_to.push_back(pend);
      }
      //  Keep instances sorted by handle.
      std::sort(write_to.begin(), write_to.end(), InstanceHandleLess{});
      pending_ui_submit.clear();
    }
  }

  (void) instance_accessor.writer_update();
}

TriggeredBufferRenderer::PendingModification&
TriggeredBufferRenderer::require_ui_pending_modification(TriggeredBufferHandle buffer_handle) {
  auto& pend_mods = ui_pending_modifications;
  if (auto it = pend_mods.find(buffer_handle); it != pend_mods.end()) {
    //  Already exists.
    return it->second;
  } else {
    PendingModification pending{};
    pending.handle = buffer_handle;
    pend_mods[buffer_handle] = pending;
    return pend_mods.at(buffer_handle);
  }
}

void TriggeredBufferRenderer::ui_set_gain(TriggeredBufferHandle buffer_handle, float gain) {
  require_ui_pending_modification(buffer_handle).gain = gain;
}

void TriggeredBufferRenderer::ui_set_modification(PendingModification&& mod) {
  auto handle = mod.handle;
  require_ui_pending_modification(handle) = std::move(mod);
}

GROVE_NAMESPACE_END
