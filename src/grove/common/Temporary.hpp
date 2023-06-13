#pragma once

#include <memory>

namespace grove {

template <typename T>
struct TemporaryView {
  T* require(int count) {
    if (count > stack_size) {
      heap = std::make_unique<T[]>(count);
      return heap.get();
    } else {
      return stack;
    }
  }

  T* stack;
  std::unique_ptr<T[]>& heap;
  int stack_size;
};

template <typename T>
struct TemporaryViewStack {
  T* begin() {
    return size <= stack_capacity ? stack : heap.get();
  }
  T* end() {
    return size <= stack_capacity ? (stack + size) : (heap.get() + size);
  }

  T* push(int count) {
    if (size + count <= stack_capacity) {
      auto curr_size = size;
      size += count;
      return stack + curr_size;

    } else if (size + count <= heap_capacity) {
      auto curr_size = size;
      size += count;
      return heap.get() + curr_size;

    } else if (size <= stack_capacity) {
      //  resize, copy from stack to heap.
      heap_capacity = stack_capacity == 0 ? 2 : stack_capacity * 2;
      while (heap_capacity < size + count) {
        heap_capacity *= 2;
      }
      heap = std::make_unique<T[]>(heap_capacity);
      std::copy(stack, stack + size, heap.get());
      auto curr_size = size;
      size += count;
      return heap.get() + curr_size;

    } else {
      //  resize, copy from heap to heap.
      heap_capacity = heap_capacity == 0 ? 2 : heap_capacity * 2;
      while (heap_capacity < size + count) {
        heap_capacity *= 2;
      }
      auto dst = std::make_unique<T[]>(heap_capacity);
      std::copy(heap.get(), heap.get() + size, dst.get());
      heap = std::move(dst);
      auto curr_size = size;
      size += count;
      return heap.get() + curr_size;
    }
  }

  T* stack;
  std::unique_ptr<T[]>& heap;
  int size;
  int stack_capacity;
  int heap_capacity;
};

template <typename T, int StackSize>
struct Temporary {
  TemporaryViewStack<T> view_stack() {
    return TemporaryViewStack<T>{stack, heap, 0, StackSize, 0};
  }

  TemporaryView<T> view() {
    return TemporaryView<T>{stack, heap, StackSize};
  }

  T* require(int count) {
    if (count > StackSize) {
      heap = std::make_unique<T[]>(count);
      return heap.get();
    } else {
      return stack;
    }
  }

  T stack[StackSize];
  std::unique_ptr<T[]> heap;
};

}