#include "UIAudioBufferStore.hpp"
#include "grove/common/common.hpp"
#include "grove/common/vector_util.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename Pending>
void update_pending(Pending&& pending) {
  DynamicArray<int, 4> erase_inds;

  for (int i = 0; i < int(pending.size()); i++) {
    auto& pend = pending[i];

    if (pend.future->is_ready()) {
      if (pend.callback) {
        pend.callback(pend.future->data);
      }

      erase_inds.push_back(i);
    }
  }

  erase_set(pending, erase_inds);
}

} //  anon

void UIAudioBufferStore::on_buffer_available(BufferAvailableFuture future,
                                             OnBufferAvailable callback) {
  PendingAvailability pending{};
  pending.callback = std::move(callback);
  pending.future = std::move(future);
  pending_availability.push_back(std::move(pending));
}

void UIAudioBufferStore::on_buffer_removed(BufferRemovedFuture future,
                                           OnBufferRemoved callback) {
  PendingRemoval pending{};
  pending.callback = std::move(callback);
  pending.future = std::move(future);
  pending_removal.push_back(std::move(pending));
}

void UIAudioBufferStore::update() {
  update_pending(pending_availability);
  update_pending(pending_removal);
}

GROVE_NAMESPACE_END
