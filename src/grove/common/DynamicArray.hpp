#pragma once

#include <cstdint>
#include <cassert>
#include <utility>
#include <algorithm>
#include <new>

#define TEMPLATE_HEADER template <typename T, int N>
#define DYNAMIC_ARRAY DynamicArray<T, N>

namespace grove {

namespace detail {
  template <typename T>
  inline void copy_range_placement_new(const T* src_begin, const T* src_end, T* dest_begin) {
    int64_t size{src_end - src_begin};
    for (int64_t i = 0; i < size; i++) {
      new (&dest_begin[i]) T{src_begin[i]};
    }
  }

  template <typename T>
  inline void move_range_placement_new(T* src_begin, T* src_end, T* dest_begin) {
    int64_t size{src_end - src_begin};
    for (int64_t i = 0; i < size; i++) {
      new (&dest_begin[i]) T{std::move(src_begin[i])};
      src_begin[i].~T();
    }
  }

  template <typename T, int N, typename U = typename std::enable_if<std::is_trivial<T>::value, int>::type>
  inline void destruct_range(T*, T*, U n = 0) {
    (void) n;
  }

  template <typename T, int N>
  inline void destruct_range(typename std::enable_if<!std::is_trivial<T>::value, T*>::type begin,
                             typename std::enable_if<!std::is_trivial<T>::value, T*>::type end) {
    int64_t size{end - begin};
    for (int64_t i = 0; i < size; i++) {
      begin[i].~T();
    }
  }
}

/*
* DynamicArray - A growable array with space on the stack for N elements.
*/

TEMPLATE_HEADER
class DynamicArray {
private:
  static constexpr uint64_t heap_mask = uint64_t(1) << 63u;
  static constexpr uint64_t capacity_mask = ~(uint64_t(1) << 63u);

public:
  DynamicArray();
  ~DynamicArray();

  DynamicArray(const DynamicArray& other);
  DynamicArray& operator=(const DynamicArray& other);

  DynamicArray(DynamicArray&& other) noexcept;
  DynamicArray& operator=(DynamicArray&& other) noexcept;

  int64_t size() const noexcept;
  uint64_t capacity() const noexcept;
  bool empty() const noexcept;

  void clear() noexcept;
  void erase(const T* pos) noexcept;
  void resize(int64_t new_size) noexcept;

  template <typename U = T>
  void push_back(U&& val) noexcept;

  template <typename... Args>
  void emplace_back(Args&&... args) noexcept;

  void pop_back() noexcept;

  T& operator[](int64_t idx) noexcept;
  const T& operator[](int64_t idx) const noexcept;

  T* begin() noexcept;
  T* end() noexcept;

  const T* begin() const noexcept;
  const T* end() const noexcept;

  T* data() noexcept;
  const T* data() const noexcept;

  T& back() noexcept;
  const T& back() const noexcept;

  void append_copy(const T* other_begin, const T* other_end);
  void append_move(T* other_begin, T* other_end);

  //  @TODO
  friend void swap(DYNAMIC_ARRAY& a, DYNAMIC_ARRAY& b) noexcept;

private:
  template <typename U = T>
  void push_back_heap(U&& val) noexcept;

  template <typename U = T>
  void maybe_push_back_stack(U&& val) noexcept;

  void set_capacity(uint64_t capacity) noexcept;
  void grow_heap() noexcept;
  void initialize_heap() noexcept;

private:
  bool is_heap() const noexcept;

private:
  union Storage {
    Storage();
    ~Storage();

    T stack[N];
    T* heap;
  } storage;

  int64_t sz;
  uint64_t cap;
};

/*
* Impl
*/

//  storage
TEMPLATE_HEADER
DYNAMIC_ARRAY::Storage::Storage() {
  //
}

//  storage
TEMPLATE_HEADER
DYNAMIC_ARRAY::Storage::~Storage() {
  //
}

//  construct
TEMPLATE_HEADER
DYNAMIC_ARRAY::DynamicArray() :
  sz(0),
  cap(N) {
  //
  static_assert(N > 0, "Expected stack storage > 0.");
  //  https://en.cppreference.com/w/cpp/memory/new/operator_new
  static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
    "Alignment requirement exceeds default, and is not currently supported.");
}

//  destruct
TEMPLATE_HEADER
DYNAMIC_ARRAY::~DynamicArray() {
  detail::destruct_range<T, N>(begin(), end());

  if (is_heap()) {
    ::operator delete[](storage.heap);
    storage.heap = nullptr;
  }

  sz = 0;
  cap = 0;
}

//  copy construct
TEMPLATE_HEADER
DYNAMIC_ARRAY::DynamicArray(const DynamicArray& other) :
  sz(other.sz),
  cap(other.cap) {
  //
  if (is_heap()) {
    storage.heap = (T*) ::operator new[](capacity() * sizeof(T));
    assert(storage.heap && "Out of memory.");
  }

  detail::copy_range_placement_new(other.begin(), other.end(), begin());
}

//  move construct
TEMPLATE_HEADER
DYNAMIC_ARRAY::DynamicArray(DynamicArray&& other) noexcept :
  sz(other.sz),
  cap(other.cap) {

  if (is_heap()) {
    auto* other_ptr = other.storage.heap;

    other.storage.heap = nullptr;
    other.sz = 0;
    other.cap = 0;

    storage.heap = other_ptr;

  } else {
    //  Move stack elements.
    detail::move_range_placement_new(other.begin(), other.end(), begin());

    other.sz = 0;
    other.cap = 0;
  }
}

//  copy assign
TEMPLATE_HEADER
DYNAMIC_ARRAY& DYNAMIC_ARRAY::operator=(const DynamicArray& other) {
  assert(this != &other);
  DYNAMIC_ARRAY::~DynamicArray();

  sz = other.sz;
  cap = other.cap;

  if (is_heap()) {
    storage.heap = (T*) ::operator new[](capacity() * sizeof(T));
    assert(storage.heap && "Out of memory.");
  }

  detail::copy_range_placement_new(other.begin(), other.end(), begin());
  return *this;
}

//  move assign
TEMPLATE_HEADER
DYNAMIC_ARRAY& DYNAMIC_ARRAY::operator=(DynamicArray&& other) noexcept {
  assert(this != &other);
  DYNAMIC_ARRAY::~DynamicArray();

  sz = other.sz;
  cap = other.cap;

  if (is_heap()) {
    auto* other_ptr = other.storage.heap;
    other.storage.heap = nullptr;
    storage.heap = other_ptr;

  } else {
    detail::move_range_placement_new(other.begin(), other.end(), begin());
  }

  other.sz = 0;
  other.cap = 0;

  return *this;
}

TEMPLATE_HEADER
bool DYNAMIC_ARRAY::empty() const noexcept {
  return size() == 0;
}


TEMPLATE_HEADER
int64_t DYNAMIC_ARRAY::size() const noexcept {
  return sz;
}

TEMPLATE_HEADER
uint64_t DYNAMIC_ARRAY::capacity() const noexcept {
  return cap & capacity_mask;
}

TEMPLATE_HEADER
bool DYNAMIC_ARRAY::is_heap() const noexcept {
  return cap & heap_mask;
}

TEMPLATE_HEADER
template <typename... Args>
void DYNAMIC_ARRAY::emplace_back(Args&&... args) noexcept {
  push_back(T{std::forward<Args>(args)...});
}

TEMPLATE_HEADER
template <typename U>
void DYNAMIC_ARRAY::push_back(U&& val) noexcept {
  if (is_heap()) {
    push_back_heap(std::forward<U>(val));
  } else {
    maybe_push_back_stack(std::forward<U>(val));
  }
}

TEMPLATE_HEADER
template <typename U>
void DYNAMIC_ARRAY::push_back_heap(U&& val) noexcept {
  if (size() == int64_t(capacity())) {
    grow_heap();
  }

  new (&storage.heap[sz++]) T{std::forward<U>(val)};
}

TEMPLATE_HEADER
template <typename U>
void DYNAMIC_ARRAY::maybe_push_back_stack(U&& val) noexcept {
  if (size() < N) {
    //  Free space in stack storage.
    new (&storage.stack[sz++]) T{std::forward<U>(val)};
  } else {
    //  Initialize heap storage.
    initialize_heap();
    new (&storage.heap[sz++]) T{std::forward<U>(val)};
  }
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::pop_back() noexcept {
  assert(size() > 0);
  begin()[size()-1].~T();
  sz--;
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::clear() noexcept {
  detail::destruct_range<T, N>(begin(), end());
  sz = 0;
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::erase(const T* pos) noexcept {
  auto* beg = begin();
  const auto index = pos - beg;
  assert(index >= 0 && index < size());

  pos->~T();

  for (int64_t i = index; i < size()-1; i++) {
    new (&beg[i]) T{std::move(beg[i + 1])};
    beg[i + 1].~T();
  }

  sz--;
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::resize(int64_t new_size) noexcept {
  assert(new_size >= 0);
  const int64_t real_sz = size();
  if (new_size > real_sz) {
    for (int64_t i = 0; i < (new_size - real_sz); i++) {
      push_back({});
    }
  } else if (new_size < real_sz) {
    for (int64_t i = 0; i < (real_sz - new_size); i++) {
      pop_back();
    }
  }
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::set_capacity(uint64_t capacity) noexcept {
  capacity &= capacity_mask;
  capacity |= (cap & heap_mask);
  cap = capacity;
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::grow_heap() noexcept {
  auto actual_capacity = capacity();

  auto* curr_begin = storage.heap;
  auto* curr_end = curr_begin + actual_capacity;

  auto grown_capacity = actual_capacity * 2;
  auto* grown = (T*) ::operator new[](grown_capacity * sizeof(T));
  assert(grown && "Out of memory.");

  detail::move_range_placement_new(curr_begin, curr_end, grown);
  ::operator delete[](storage.heap);
  storage.heap = grown;
  set_capacity(grown_capacity);
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::initialize_heap() noexcept {
  auto* curr_begin = storage.stack;
  auto* curr_end = curr_begin + N;

  auto grown_capacity = N * 2;
  auto* grown = (T*) ::operator new[](grown_capacity * sizeof(T));
  assert(grown && "Out of memory.");

  detail::move_range_placement_new(curr_begin, curr_end, grown);
  storage.heap = grown;

  cap |= heap_mask;
  set_capacity(grown_capacity);
}

TEMPLATE_HEADER
T& DYNAMIC_ARRAY::operator[](int64_t idx) noexcept {
  assert(idx >= 0 && idx < size());
  return begin()[idx];
}

TEMPLATE_HEADER
const T& DYNAMIC_ARRAY::operator[](int64_t idx) const noexcept {
  assert(idx >= 0 && idx < size());
  return begin()[idx];
}

TEMPLATE_HEADER
T* DYNAMIC_ARRAY::begin() noexcept {
  if (is_heap()) {
    return storage.heap;
  } else {
    return storage.stack;
  }
}

TEMPLATE_HEADER
T* DYNAMIC_ARRAY::end() noexcept {
  return begin() + size();
}

TEMPLATE_HEADER
const T* DYNAMIC_ARRAY::begin() const noexcept {
  if (is_heap()) {
    return storage.heap;
  } else {
    return storage.stack;
  }
}

TEMPLATE_HEADER
const T* DYNAMIC_ARRAY::end() const noexcept {
  return begin() + size();
}

TEMPLATE_HEADER
T* DYNAMIC_ARRAY::data() noexcept {
  return begin();
}

TEMPLATE_HEADER
const T* DYNAMIC_ARRAY::data() const noexcept {
  return begin();
}

TEMPLATE_HEADER
T& DYNAMIC_ARRAY::back() noexcept {
  return (*this)[size()-1];
}

TEMPLATE_HEADER
const T& DYNAMIC_ARRAY::back() const noexcept {
  return (*this)[size()-1];
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::append_copy(const T* other_begin, const T* other_end) {
  const int64_t num = other_end - other_begin;
  for (int64_t i = 0; i < num; i++) {
    push_back(other_begin[i]);
  }
}

TEMPLATE_HEADER
void DYNAMIC_ARRAY::append_move(T* other_begin, T* other_end) {
  const int64_t num = other_end - other_begin;
  for (int64_t i = 0; i < num; i++) {
    push_back(std::move(other_begin[i]));
  }
}

TEMPLATE_HEADER
void swap(DYNAMIC_ARRAY& a, DYNAMIC_ARRAY& b) noexcept {
  auto tmp = std::move(a);
  a = std::move(b);
  b = std::move(tmp);
}

#undef TEMPLATE_HEADER
#undef DYNAMIC_ARRAY

}