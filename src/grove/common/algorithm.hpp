#pragma once

#include <algorithm>
#include <cassert>

namespace grove::alg {

template <typename T>
const T* median3(const T* a, const T* b, const T* c) {
  if (*b < *a) {
    std::swap(a, b);
  }
  if (*c < *b) {
    std::swap(b, c);
  }
  return b;
}

template <typename T>
[[maybe_unused]] void detail_validate_quick_select_ranges(const T* ab, const T* lt_end,
                                                          const T* gt_begin, const T* ae,
                                                          const T& p) {
  (void) p;
  assert(ab <= lt_end && lt_end <= gt_begin && gt_begin <= ae);
  for (auto* it = ab; it != lt_end; ++it) {
    assert(*it < p);
  }
  for (auto* it = lt_end; it != gt_begin; ++it) {
    assert(*it == p);
  }
  for (auto* it = gt_begin; it != ae; ++it) {
    assert(*it > p);
  }
}

/*
 * [1] Tibshirani, R. J. (2008). Fast computation of the median by successive binning.
 *
 * Note: The following is the simple quick select alg described in this reference, not the paper's
 * binning method.
 */

template <typename T>
T* quick_select_in_place(T* begin, T* end, int64_t k) {
  using std::swap;

  const size_t size = end - begin;
  if (size == 0) {
    return end;
  }

  T* ab = begin;
  T* ae = end;
  while (true) {
    assert(k > 0);
    assert(ae >= ab);

    const T p = *median3(ab, ab + (ae - ab) / 2, ab + (ae - ab) - 1);

    auto* i = ab;
    auto* j = ae - 1;
    while (i <= j) {
      if (*i < p) {
        i++;
      } else {
        swap(*i, *j);
        j--;
      }
    }

    auto* lt_end = i;

    j = ae - 1;
    while (i <= j) {
      if (*j > p) {
        j--;
      } else {
        swap(*i, *j);
        i++;
      }
    }

    auto* gt_begin = i;
#ifdef GROVE_DEBUG
    detail_validate_quick_select_ranges(ab, lt_end, gt_begin, ae, p);
#endif

    auto len_a1 = lt_end - ab;
    auto len_a2 = gt_begin - lt_end;

    if (k <= len_a1) {
      ae = lt_end;

    } else if (k > len_a1 + len_a2) {
      ab = gt_begin;
      k = k - len_a1 - len_a2;

    } else {
      return lt_end;
    }
  }
}

}