#include "memory.hpp"
#include "./common.hpp"
#include "./platform.hpp"
#include <cassert>

#ifndef GROVE_WIN
#include <cstdlib>
#endif

GROVE_NAMESPACE_BEGIN

//  https://github.com/SaschaWillems/Vulkan/blob/master/examples/dynamicuniformbuffer/dynamicuniformbuffer.cpp

void* aligned_malloc(size_t size, size_t align) {
  void* data{};
#ifdef GROVE_WIN
  data = _aligned_malloc(size, align);
#else
#ifdef GROVE_MACOS
  data = aligned_alloc(align, size);
#else
  int res = posix_memalign(&data, align, size);
  if (res != 0) {
    data = nullptr;
  }
#endif
#endif
  return data;
}

void aligned_free(void* data) {
#ifdef GROVE_WIN
  _aligned_free(data);
#else
  free(data);
#endif
}

UniquePtrWithDeleter<void> make_aligned(size_t size, size_t align) {
  return UniquePtrWithDeleter{aligned_malloc(size, align), aligned_free};
}

UniquePtrWithDeleter<void> make_aligned_if_non_zero(size_t size, size_t align) {
  if (align > 0) {
    return make_aligned(size, align);
  } else {
    return UniquePtrWithDeleter{malloc(size), free};
  }
}

unsigned char* allocate(unsigned char** begin, size_t* space, size_t size, size_t align) {
  void* v = *begin;
  auto res = std::align(align, size, v, *space);
  if (res) {
    *begin = static_cast<unsigned char*>(v);
    *begin += size;
    *space -= size;
  }
  return static_cast<unsigned char*>(res);
}

bool sub_allocate(unsigned char** data, size_t* data_size,
                  const size_t* sub_sizes, unsigned char** dsts, size_t num_subs) {
  auto orig_data = *data;
  size_t orig_size = *data_size;
  for (size_t i = 0; i < num_subs; i++) {
    if (auto beg = allocate(data, data_size, sub_sizes[i], 1)) {
      dsts[i] = beg;
    } else {
      *data = orig_data;
      *data_size = orig_size;
      return false;
    }
  }
  return true;
}

bool make_linear_allocators(unsigned char** data, size_t* data_size, const size_t* sub_sizes,
                            LinearAllocator* allocs, size_t num_allocs) {
  auto orig_data = *data;
  auto orig_size = *data_size;
  for (uint32_t i = 0; i < num_allocs; i++) {
    if (auto beg = allocate(data, data_size, sub_sizes[i], 1)) {
      allocs[i] = make_linear_allocator(beg, beg + sub_sizes[i]);
    } else {
      *data = orig_data;
      *data_size = orig_size;
      return false;
    }
  }
  return true;
}

std::unique_ptr<unsigned char[]>
make_linear_allocators_from_heap(const size_t* sizes, LinearAllocator* allocs, size_t num_allocs,
                                 size_t* full_size) {
  size_t s{};
  for (size_t i = 0; i < num_allocs; i++) {
    s += sizes[i];
  }
  if (full_size) {
    *full_size = s;
  }
  auto result = std::make_unique<unsigned char[]>(s);
  unsigned char* data = result.get();
  if (make_linear_allocators(&data, &s, sizes, allocs, num_allocs)) {
    assert(s == 0);
  } else {
    assert(false);
  }
  return result;
}

GROVE_NAMESPACE_END
