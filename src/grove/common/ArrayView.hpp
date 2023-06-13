#pragma once

#include <cstdint>
#include <utility>
#include <cassert>
#include <vector>

namespace grove {

template <typename T, int N>
class DynamicArray;

template <typename T>
class ArrayView {
public:
  ArrayView();
  ArrayView(T* begin, T* end);

  const T& operator[](int64_t off) const;
  T& operator[](int64_t off);

  int64_t size() const;
  bool empty() const {
    return size() == 0;
  }

  T* begin();
  T* end();

  const T* begin() const;
  const T* end() const;

  T* data() {
    return beg_;
  }
  const T* data() const {
    return beg_;
  }

private:
  T* beg_;
  T* end_;
};

template <typename T, typename U>
ArrayView<T> make_iterator_array_view(U&& source) {
  return ArrayView<T>(std::begin(source), std::end(source));
}

template <typename T, typename U>
ArrayView<T> make_data_array_view(U&& source) {
  return ArrayView<T>(std::forward<U>(source).data(),
                      std::forward<U>(source).data() + std::forward<U>(source).size());
}

template <typename T>
ArrayView<const T> make_view(const std::vector<T>& v) {
  return ArrayView<const T>{v.data(), v.data() + v.size()};
}

template <typename T, int N>
ArrayView<const T> make_view(const DynamicArray<T, N>& v) {
  return ArrayView<const T>{v.data(), v.data() + v.size()};
}

template <typename T>
ArrayView<T> make_mut_view(std::vector<T>& v) {
  return ArrayView<T>{v.data(), v.data() + v.size()};
}

template <typename T, int N>
ArrayView<T> make_mut_view(DynamicArray<T, N>& v) {
  return ArrayView<T>{v.data(), v.data() + v.size()};
}

/*
 * Impl
 */

template <typename T>
ArrayView<T>::ArrayView() :
  beg_(nullptr),
  end_(nullptr) {
  //
}

template <typename T>
ArrayView<T>::ArrayView(T* beg, T* end) :
  beg_(beg),
  end_(end) {
  //
}

template <typename T>
int64_t ArrayView<T>::size() const {
  return end_ - beg_;
}

template <typename T>
T* ArrayView<T>::begin() {
  return beg_;
}

template <typename T>
const T* ArrayView<T>::begin() const {
  return beg_;
}

template <typename T>
T* ArrayView<T>::end() {
  return end_;
}

template <typename T>
const T* ArrayView<T>::end() const {
  return end_;
}

template <typename T>
const T& ArrayView<T>::operator[](int64_t off) const {
  assert(off >= 0 && off < size());
  return begin()[off];
}

template <typename T>
T& ArrayView<T>::operator[](int64_t off) {
  assert(off >= 0 && off < size());
  return begin()[off];
}

}