#pragma once

#include "DynamicArray.hpp"
#include <vector>

namespace grove {

template <typename T, typename U>
void erase_set(std::vector<T>& from, const U* begin_erase_ind, const U* end_erase_ind) {
  U offset = 0;

  for (auto erase_ind = begin_erase_ind; erase_ind != end_erase_ind; ++erase_ind) {
    auto begin = from.begin();
    auto to_erase = begin + (*erase_ind - offset);
    from.erase(to_erase);
    offset++;
  }
}

template <typename T, typename U>
void erase_set(std::vector<T>& from, const U& inds) {
  auto offset = 0;

  for (const auto& ind : inds) {
    auto begin = from.begin();
    auto to_erase = begin + (ind - offset);
    from.erase(to_erase);
    offset++;
  }
}

template <typename T, int N, typename U>
void erase_set(DynamicArray<T, N>& from, const U& inds) {
  auto offset = 0;

  for (const auto& ind : inds) {
    auto begin = from.begin();
    auto to_erase = begin + (ind - offset);
    from.erase(to_erase);
    offset++;
  }
}

//  Copy every i-th element from `Source` to `Dest`, unless `i` is in the set of indices
//  given by `excluding`.
template <typename Source, typename Dest, typename Indices>
void copy_into_excluding(const Source& source, Dest& dest, const Indices& excluding) {
  const int64_t size = source.size();
  for (int64_t i = 0; i < size; i++) {
    if (excluding.count(i) == 0) {
      dest.push_back(source[i]);
    }
  }
}

}