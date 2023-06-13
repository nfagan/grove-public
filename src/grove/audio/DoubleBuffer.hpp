#pragma once

#include "grove/common/config.hpp"
#include <atomic>
#include <cassert>
#include <algorithm>

namespace grove::audio {

namespace detail {
  template <typename T>
  struct DoubleBufferAsSetTraits {
    static constexpr bool enable_mutable_read() {
      return false;
    }

    template <typename U>
    static void add(T& array, U&& value) noexcept {
#ifdef GROVE_DEBUG
      auto it = std::find(array.begin(), array.end(), std::forward<U>(value));
      assert(it == array.end());
#endif
      array.push_back(std::forward<U>(value));
    }

    template <typename U>
    static void remove(T& array, U&& value) noexcept {
      auto it = std::find(array.begin(), array.end(), std::forward<U>(value));
      assert(it != array.end());
      if (it != array.end()) {
        array.erase(it);
      }
    }

    static T* on_reader_swap(T* write_to, T* read_from) noexcept {
      *write_to = *read_from;
      return write_to;
    }
  };
}

template <typename T>
struct DoubleBuffer {
  T a;
  T b;
};

template <typename T, typename Traits = detail::DoubleBufferAsSetTraits<T>>
class DoubleBufferAccessor : public Traits {
  enum class WriteState {
    None,
    AwaitingSubmit,
    AwaitingSwap
  };

public:
  struct WriterUpdateResult {
    bool changed{false};
    T* changed_to{};
  };

public:
  explicit DoubleBufferAccessor(DoubleBuffer<T>& buffer) :
    DoubleBufferAccessor{&buffer.a, &buffer.b} {
    //
  }

  DoubleBufferAccessor(T* a, T* b) : write_to{a}, read_from{b} {
    assert(write_to != read_from);
  }

  bool writer_can_modify() const noexcept {
    return write_state != WriteState::AwaitingSwap;
  }

  T* writer_begin_modification() noexcept {
    if (write_state == WriteState::AwaitingSwap) {
      return nullptr;

    } else {
      write_state = WriteState::AwaitingSubmit;
      return write_to;
    }
  }

  template <typename... Args>
  bool writer_modify(Args&&... args) noexcept {
    if (write_state == WriteState::AwaitingSwap) {
      return false;

    } else {
      Traits::modify(*write_to, std::forward<Args>(args)...);
      write_state = WriteState::AwaitingSubmit;
      return true;
    }
  }

  template <typename U>
  bool writer_add(U&& value) noexcept {
    if (write_state == WriteState::AwaitingSwap) {
      return false;

    } else {
      Traits::add(*write_to, std::forward<U>(value));
      write_state = WriteState::AwaitingSubmit;
      return true;
    }
  }

  template <typename U>
  bool writer_remove(U&& value) noexcept {
    if (write_state == WriteState::AwaitingSwap) {
      return false;

    } else {
      Traits::remove(*write_to, std::forward<U>(value));
      write_state = WriteState::AwaitingSubmit;
      return true;
    }
  }

  WriterUpdateResult writer_update() noexcept {
    WriterUpdateResult result{};

    if (write_state == WriteState::AwaitingSwap) {
      bool maybe_swapped{true};

      if (swapped.compare_exchange_strong(maybe_swapped, false)) {
        write_to = Traits::on_reader_swap(write_to, read_from);
        write_state = WriteState::None;

        result.changed = true;
        result.changed_to = write_to;
      }

    } else if (write_state == WriteState::AwaitingSubmit) {
      submit();
    }

    return result;
  }

  void reader_maybe_swap() noexcept {
    bool maybe_changed{true};
    if (changed.compare_exchange_strong(maybe_changed, false)) {
      std::swap(write_to, read_from);
      swapped.store(true);
    }
  }

  const T& maybe_swap_and_read() noexcept {
    reader_maybe_swap();
    return read();
  }

  //  @Note: This method is potentially unsafe. Using the default accessor traits, the reading
  //  thread's data are copied *by the writing thread* after the reading thread has swapped
  //  read and write pointers. Thus, the reading thread can only modify T if copying T is thread
  //  safe, or if custom accessor traits are supplied such that T is not copied after the reading
  //  thread performs the pointer swap.
  T& maybe_swap_and_read_mut() noexcept {
    static_assert(Traits::enable_mutable_read(), "Mutable reads must be manually enabled.");
    reader_maybe_swap();
    return *read_from;
  }

  const T& read() const noexcept {
    return *read_from;
  }

  //  @Note: Only safe to call if writer_can_modify() returns true.
  const T* writer_ptr() const noexcept {
    return write_to;
  }

  //  @Note: Only safe to call if writer_can_modify() returns true.
  T* writer_ptr() noexcept {
    return write_to;
  }

private:
  void submit() noexcept {
    assert(!changed.load());
    write_state = WriteState::AwaitingSwap;
    changed.store(true);
  }

private:
  T* write_to;
  T* read_from;

  std::atomic<bool> changed{false};
  std::atomic<bool> swapped{false};

  WriteState write_state{};
};

}