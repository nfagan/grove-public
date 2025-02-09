#pragma once

#include <cstddef>
#include <utility>
#include <memory>
#include <cassert>

namespace grove {

template <typename T>
struct UniquePtrWithDeleter {
  using Deleter = void(T*);

  UniquePtrWithDeleter() noexcept = default;
  explicit UniquePtrWithDeleter(T* data, Deleter* deleter) noexcept :
    data{data},
    deleter{deleter} {
    //
  }
  UniquePtrWithDeleter(const UniquePtrWithDeleter& other) = delete;
  UniquePtrWithDeleter& operator=(const UniquePtrWithDeleter& other) = delete;
  UniquePtrWithDeleter(UniquePtrWithDeleter&& other) noexcept :
    data{other.data},
    deleter{other.deleter} {
    other.data = nullptr;
    other.deleter = nullptr;
  }
  UniquePtrWithDeleter& operator=(UniquePtrWithDeleter&& other) noexcept {
    UniquePtrWithDeleter tmp{std::move(other)};
    swap(*this, tmp);
    return *this;
  }
  ~UniquePtrWithDeleter() {
    if (deleter) {
      deleter(data);
    }
  }
  friend inline void swap(UniquePtrWithDeleter& a, UniquePtrWithDeleter& b) noexcept {
    using std::swap;
    swap(a.data, b.data);
    swap(a.deleter, b.deleter);
  }

  T* data{};
  Deleter* deleter{};
};

inline size_t aligned_element_size(size_t element_size, size_t align) {
  auto div = element_size / align;
  auto mod = element_size % align;
  auto sz = div + (mod == 0 ? 0 : 1);
  return sz * align;
}

inline size_t aligned_element_size_check_zero(size_t element_size, size_t min_align) {
  if (min_align > 0) {
    return aligned_element_size(element_size, min_align);
  } else {
    return element_size;
  }
}

void* aligned_malloc(size_t size, size_t align);
void aligned_free(void* data);
UniquePtrWithDeleter<void> make_aligned(size_t size, size_t align);
UniquePtrWithDeleter<void> make_aligned_if_non_zero(size_t size, size_t align);

unsigned char* allocate(unsigned char** begin, size_t* space, size_t size, size_t align);
[[nodiscard]] bool sub_allocate(unsigned char** data, size_t* data_size,
                                const size_t* sub_sizes, unsigned char** dsts, size_t num_subs);

struct LinearAllocator {
  unsigned char* begin;
  unsigned char* end;
  unsigned char* p;
};

inline LinearAllocator make_linear_allocator(unsigned char* beg, unsigned char* end) {
  return LinearAllocator{beg, end, beg};
}

[[nodiscard]] bool make_linear_allocators(unsigned char** data, size_t* data_size,
                                          const size_t* sub_sizes, LinearAllocator* allocs,
                                          size_t num_allocs);

inline unsigned char* allocate(LinearAllocator* alloc, size_t size, size_t align) {
  size_t s = alloc->end - alloc->p;
  return allocate(&alloc->p, &s, size, align);
}

template <typename T>
inline unsigned char* allocate_n(LinearAllocator* alloc, size_t count) {
  return allocate(alloc, sizeof(T) * count, 1);
}

template <typename T>
inline unsigned char* aligned_allocate_n(LinearAllocator* alloc, size_t count) {
  return allocate(alloc, sizeof(T) * count, alignof(T));
}

inline void clear(LinearAllocator* alloc) {
  alloc->p = alloc->begin;
}

inline void zero_memory(unsigned char* data, size_t size) {
  memset(data, 0, size);
}

template <typename T>
inline void zero_memory_n(unsigned char* data, size_t count) {
  zero_memory(data, sizeof(T) * count);
}

template <typename T>
void read_ith(T* dst, const unsigned char* data, size_t i) {
  memcpy(dst, data + i * sizeof(T), sizeof(T));
}

template <typename T>
void write_ith(unsigned char* data, const T* dst, size_t i) {
  memcpy(data + i * sizeof(T), dst, sizeof(T));
}

template <typename T>
void push(LinearAllocator* alloc, const T* elements, uint64_t num_elements) {
  auto size = sizeof(T) * num_elements;
  auto res = allocate(alloc, size, 1);
  assert(res);
  memcpy(res, elements, size);
}

template <typename T>
void aligned_push(LinearAllocator* alloc, const T* elements, uint64_t num_elements) {
  auto size = sizeof(T) * num_elements;
  auto res = allocate(alloc, size, alignof(T));
  assert(res);
  memcpy(res, elements, size);
}

inline size_t size(const LinearAllocator* alloc) {
  return alloc->p - alloc->begin;
}

template <typename T>
size_t count_elements(const LinearAllocator* alloc) {
  auto res = size(alloc) / sizeof(T);
#ifdef GROVE_DEBUG
  assert(res * sizeof(T) == size(alloc));
#endif
  return res;
}

std::unique_ptr<unsigned char[]>
make_linear_allocators_from_heap(const size_t* sizes, LinearAllocator* allocs, size_t num_allocs,
                                 size_t* full_size = nullptr);

}