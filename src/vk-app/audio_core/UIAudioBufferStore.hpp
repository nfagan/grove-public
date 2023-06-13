#pragma once

#include "grove/common/Future.hpp"
#include "grove/audio/AudioBufferStore.hpp"

namespace grove {

class UIAudioBufferStore {
public:
  using OnBufferAvailable = std::function<void(AudioBufferHandle)>;
  using OnBufferRemoved = std::function<void(AudioBufferStore::RemoveResult)>;

  using BufferAvailableFuture = std::unique_ptr<Future<AudioBufferHandle>>;
  using BufferRemovedFuture = std::unique_ptr<Future<AudioBufferStore::RemoveResult>>;

  struct PendingAvailability {
    BufferAvailableFuture future;
    OnBufferAvailable callback;
  };

  struct PendingRemoval {
    BufferRemovedFuture future;
    OnBufferRemoved callback;
  };

public:
  void update();

  void on_buffer_available(BufferAvailableFuture future, OnBufferAvailable callback = nullptr);
  void on_buffer_removed(BufferRemovedFuture future, OnBufferRemoved callback = nullptr);

private:
  DynamicArray<PendingAvailability, 4> pending_availability;
  DynamicArray<PendingRemoval, 4> pending_removal;
};

}