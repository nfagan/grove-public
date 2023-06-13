#pragma once

#include <array>
#include <cmath>

namespace grove {

template <typename T, int N>
class History {
public:
  History();

  void push(const T& value);
  T mean() const;
  T mean_or_default(const T& v) const;
  T max_or_default(const T& v) const;
  T min_or_default(const T& v) const;
  T std_or_default(const T& v = T(1)) const;
  T var_or_default(const T& v = T(1)) const;
  T latest() const;
  int num_samples() const {
    return size;
  }

  T* begin() {
    return history.data();
  }

  T* end() {
    return history.data() + size;
  }

private:
  std::array<T, N> history;
  int size;
};

/*
 * Impl
 */

template <typename T, int N>
History<T, N>::History() : size(0) {
  static_assert(N > 0, "Expected N > 0.");
}

template <typename T, int N>
void History<T, N>::push(const T& value) {
  if (size == N) {
    for (int i = 1; i < size; i++) {
      history[i-1] = std::move(history[i]);
    }
    history[size-1] = value;

  } else {
    history[size++] = value;
  }
}

template <typename T, int N>
T History<T, N>::mean() const {
  if (size == 0) {
    return T(0);
  }

  T mean{0};

  for (int i = 0; i < size; i++) {
    mean += history[i];
  }

  return mean / T(size);
}

template <typename T, int N>
T History<T, N>::mean_or_default(const T& v) const {
  return size == 0 ? v : mean();
}

template <typename T, int N>
T History<T, N>::std_or_default(const T& v) const {
  return size == 0 ? v : std::sqrt(var_or_default(v));
}

template <typename T, int N>
T History<T, N>::var_or_default(const T& v) const {
  if (size == 0) {
    return v;

  } else {
    auto m = mean();
    T ss{};

    for (int i = 0; i < size; i++) {
      auto diff = history[i] - m;
      ss += diff * diff;
    }

    if (size > 1) {
      ss /= T(size - 1);
    }

    return ss;
  }
}

template <typename T, int N>
T History<T, N>::min_or_default(const T& v) const {
  if (size == 0) {
    return v;
  } else {
    return *std::min_element(history.begin(), history.begin() + size);
  }
}

template <typename T, int N>
T History<T, N>::max_or_default(const T& v) const {
  if (size == 0) {
    return v;
  } else {
    return *std::max_element(history.begin(), history.begin() + size);
  }
}

template <typename T, int N>
T History<T, N>::latest() const {
  if (size == 0) {
    return T(0);
  } else {
    return history[size-1];
  }
}

}