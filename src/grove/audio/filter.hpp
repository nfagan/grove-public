#pragma once

#include "grove/common/DynamicArray.hpp"
#include "grove/common/ArrayView.hpp"

namespace grove::audio {

namespace detail {
  template <typename T>
  void array_right_rotate_zero(T* y, int64_t size, int64_t num_add) {
    for (int64_t i = size-1; i >= num_add; i--) {
      y[i] = y[i-num_add];
    }

    for (int64_t i = 0; i < num_add; i++) {
      y[i] = 0;
    }
  }

  template <typename T>
  void array_left_shift(T* y, int64_t size, int64_t num_remove) {
    for (int64_t i = 0; i < size - num_remove; i++) {
      y[i] = y[i + num_remove];
    }
  }
}

template <typename Sample>
Sample linear_filter_tick(const Sample* b, int size_b,
                          const Sample* a, int size_a,
                          Sample* x, Sample* y, Sample s) {
  if (size_b == 0 || size_a == 0) {
    return s;
  }

  auto fir = s * b[0];
  for (int i = 1; i < size_b; i++) {
    fir += b[i] * x[size_b - i - 1];
  }

  auto iir = fir;
  for (int i = 1; i < size_a; i++) {
    iir -= a[i] * y[size_a - i];
  }

  for (int i = 0; i < size_b-2; i++) {
    x[i] = x[i + 1];
  }

  if (size_b > 1) {
    x[size_b - 2] = s;
  }

  for (int i = 0; i < size_a-1; i++) {
    y[i] = y[i + 1];
  }

  auto out = iir * a[0];
  y[size_a - 1] = out;
  return out;
}

template <typename Sample, int StackSizeA = 1, int StackSizeB = 1>
class LinearFilter {
public:
  void resize(int num_b, int num_a) {
    resize_b(num_b);
    resize_a(num_a);
  }

  void resize_b(int size) {
    set_b(nullptr, size);
  }

  void resize_a(int size) {
    set_a(nullptr, size);
  }

  void set_b(const Sample* coeff, int size);
  void set_a(const Sample* coeff, int size);

  ArrayView<Sample> get_b() {
    return {b.data(), b.size()};
  }

  ArrayView<Sample> get_a() {
    return {a.data(), a.size()};
  }

  Sample tick(Sample s) {
    return linear_filter_tick(
      b.data(), int(b.size()), a.data(), int(a.size()), x.data(), y.data(), s);
  }

  void process(Sample* in_out, int size, int off = 0, int stride = 1);

private:
  DynamicArray<Sample, StackSizeA> a;
  DynamicArray<Sample, StackSizeB> b;
  DynamicArray<Sample, StackSizeB> x;
  DynamicArray<Sample, StackSizeA> y;
};

template <typename Sample, int A, int B>
inline void LinearFilter<Sample, A, B>::process(Sample* in_out, int size, int off, int stride) {
  for (int i = 0; i < size; i++) {
    auto* p = in_out + stride * i + off;
    *p = tick(*p);
  }
}

template <typename Sample, int A, int B>
inline void LinearFilter<Sample, A, B>::set_a(const Sample* coeff, int size) {
  a.clear();

  if (coeff) {
    for (int i = 0; i < size; i++) {
      a.push_back(coeff[i]);
    }
  } else {
    for (int i = 0; i < size; i++) {
      a.push_back(Sample(0));
    }
  }

  int num_add = size - int(y.size());
  if (num_add > 0) {
    for (int i = 0; i < num_add; i++) {
      y.emplace_back();
    }
    detail::array_right_rotate_zero(y.data(), y.size(), num_add);

  } else if (num_add < 0) {
    auto num_rem = -num_add;
    detail::array_left_shift(y.data(), y.size(), num_rem);
    for (int i = 0; i < num_rem; i++) {
      y.pop_back();
    }
  }
}

template <typename Sample, int A, int B>
inline void LinearFilter<Sample, A, B>::set_b(const Sample* coeff, int size) {
  b.clear();

  if (coeff) {
    for (int i = 0; i < size; i++) {
      b.push_back(coeff[i]);
    }
  } else {
    for (int i = 0; i < size; i++) {
      b.push_back(Sample(0));
    }
  }

  int num_add = (size - 1) - int(x.size());
  if (num_add > 0) {
    for (int i = 0; i < num_add; i++) {
      x.emplace_back();
    }
    detail::array_right_rotate_zero(x.data(), x.size(), num_add);

  } else if (num_add < 0) {
    auto num_rem = -num_add;
    detail::array_left_shift(x.data(), x.size(), num_rem);
    for (int i = 0; i < num_rem; i++) {
      x.pop_back();
    }
  }
}

}