#pragma once

#include "AudioRenderable.hpp"
#include "DoubleBuffer.hpp"
#include "audio_buffer.hpp"
#include "envelope.hpp"
#include "grove/common/identifier.hpp"
#include "grove/common/Optional.hpp"
#include "grove/common/RingBuffer.hpp"
#include <unordered_map>

namespace grove {

class AudioBufferStore;

struct TriggeredBufferHandle {
  bool is_valid() const {
    return id != 0;
  }

  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TriggeredBufferHandle, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TriggeredBufferHandle, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(TriggeredBufferHandle, id)

  uint32_t id{};
};

enum class TriggeredBufferLoopType {
  None,
  Forwards
};

namespace impl {
  struct TriggeredBufferSharedState {
    std::atomic<bool> abort_triggered{false};
    std::atomic<bool> expired{false};
    double frame_index{};
    audio::ExpInterpolated<float> gain{1.0f};
    float timeout_s{};
  };
}

class UITriggeredBufferInstance {
  friend class TriggeredBufferRenderer;
public:
  bool is_valid() const {
    return handle.is_valid() && state != nullptr;
  }

  void stop() {
    if (is_valid()) {
      state->abort_triggered.store(true);
    }
  }

  bool is_expired() const {
    return !is_valid() || state->expired.load();
  }

  TriggeredBufferHandle get_handle() const {
    return handle;
  }

private:
  TriggeredBufferHandle handle{};
  std::shared_ptr<impl::TriggeredBufferSharedState> state{};
};

struct TriggeredBufferPlayParams {
  TriggeredBufferLoopType loop_type{TriggeredBufferLoopType::None};
  double playback_rate_multiplier{1.0};
  float gain{1.0f};
  bool fade_out{};
  float timeout_s{};
};

class TriggeredBufferRenderer : public AudioRenderable {
public:
  struct Instance {
    TriggeredBufferHandle instance_handle{};
    AudioBufferHandle buffer_handle{};
    std::shared_ptr<impl::TriggeredBufferSharedState> state{};

    double playback_rate_multiplier{1.0};
    TriggeredBufferLoopType loop_type{TriggeredBufferLoopType::None};
    bool fade_out{};
  };

  template <typename T>
  using HandleMap = std::unordered_map<TriggeredBufferHandle, T, TriggeredBufferHandle::Hash>;

  using Instances_ = std::vector<Instance>;
  using Instances = audio::DoubleBuffer<Instances_>;
  using InstanceAccessor = audio::DoubleBufferAccessor<Instances_>;

  struct PendingModification {
    TriggeredBufferHandle handle;
    Optional<float> gain;
  };

  using PendingModifications = HandleMap<PendingModification>;

public:
  explicit TriggeredBufferRenderer(const AudioBufferStore* buffer_store);
  ~TriggeredBufferRenderer() override = default;

  void render(const AudioRenderer& renderer,
              Sample* samples,
              AudioEvents* events,
              const AudioRenderInfo& info) override;

  UITriggeredBufferInstance ui_play(AudioBufferHandle buffer_handle,
                                    const TriggeredBufferPlayParams& params = {});
  void ui_update();
  void ui_set_gain(TriggeredBufferHandle buffer_handle, float gain);
  void ui_set_modification(PendingModification&& mod);

private:
  PendingModification& require_ui_pending_modification(TriggeredBufferHandle buffer_handle);
  void ui_submit_pending_modifications();
  void render_apply_modifications(const Instances_& instances);

private:
  const AudioBufferStore* buffer_store;
  uint32_t next_instance_id{1};

  std::vector<Instance> pending_ui_submit;

  Instances instances;
  InstanceAccessor instance_accessor{instances};

  PendingModifications ui_pending_modifications;
  RingBuffer<PendingModification, 4> pending_modifications;
};

}