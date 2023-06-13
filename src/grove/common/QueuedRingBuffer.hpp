#pragma once

#include "RingBuffer.hpp"
#include "DynamicArray.hpp"

#define TEMPLATE_HEADER template <typename T, int RingN, int ArrayN, typename RingStorage>
#define RING_BUFFER QueuedRingBuffer<T, RingN, ArrayN, RingStorage>

namespace grove {

/*
 * QueuedRingBuffer - A ring buffer with space for RingN elements, thread-safe for 1 reader and
 * 1 writer. If RingN elements are written, additional written elements will be queued and not
 * written to the ring buffer until space becomes available. The queue is a DynamicArray with
 * space on the stack for ArrayN elements.
 */

template <typename T, int RingN, int ArrayN = 2,
          typename RingStorage = RingBufferStackStorage<T, RingN>>
class QueuedRingBuffer {
public:
  template <typename U>
  void write(U&& val) noexcept;

  T read() noexcept;
  int num_pending_read() const noexcept;

private:
  RingBuffer<T, RingN, RingStorage> ring_buffer;
  DynamicArray<T, ArrayN> queue;
};

/*
 * Impl
 */

TEMPLATE_HEADER
template <typename U>
void RING_BUFFER::write(U&& val) noexcept {
  while (!ring_buffer.full() && !queue.empty()) {
    ring_buffer.write(std::move(*queue.begin()));
    queue.erase(queue.begin());
  }

  if (!ring_buffer.full() && queue.empty()) {
    ring_buffer.write(std::forward<U>(val));
  } else {
    queue.push_back(std::forward<U>(val));
  }
}

TEMPLATE_HEADER
T RING_BUFFER::read() noexcept {
  return ring_buffer.read();
}

TEMPLATE_HEADER
int RING_BUFFER::num_pending_read() const noexcept {
  return ring_buffer.size();
}

#undef TEMPLATE_HEADER
#undef RING_BUFFER

}