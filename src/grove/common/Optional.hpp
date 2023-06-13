#pragma once

#include <utility>
#include <cassert>

namespace grove {
  template <typename T>
  class Optional;

  class NullOpt {
    //
  };
}

template <typename T>
class grove::Optional {
public:
  constexpr Optional() noexcept : val(), is_null(true) {
    //
  }

  constexpr Optional(Optional&& other) noexcept :
    val(std::move(other.val)), is_null(other.is_null) {
    //
  }

  template <typename ...Args>
  constexpr explicit Optional(Args&&... args) :
    val(std::forward<Args...>(args...)), is_null(false) {
    //
  }

  constexpr Optional(const NullOpt&) noexcept : Optional() {
    //
  }

  constexpr Optional(NullOpt&&) noexcept : Optional() {
    //
  }

  constexpr Optional(const Optional& other) : val(other.val), is_null(other.is_null) {
    //
  }

  ~Optional() = default;

  Optional& operator=(const NullOpt&) {
    val = T{};
    is_null = true;

    return *this;
  }

  Optional& operator=(const Optional& other) {
    is_null = other.is_null;
    val = other.val;

    return *this;
  }

  Optional& operator=(Optional&& other) noexcept {
    is_null = other.is_null;
    val = std::move(other.val);

    return *this;
  }

  Optional& operator=(const T& value) {
    val = value;
    is_null = false;

    return *this;
  }

  Optional& operator=(T&& value) noexcept {
    val = std::move(value);
    is_null = false;

    return *this;
  }

  operator bool() const {
    return !is_null;
  }

  bool has_value() const {
    return !is_null;
  }

  const T& value() const {
    return val;
  }

  T& value() {
    return val;
  }

  T& unwrap() {
    assert(!is_null);
    return val;
  }

  const T& unwrap() const {
    assert(!is_null);
    return val;
  }

private:
  T val;
  bool is_null;
};

inline bool operator==(const grove::NullOpt&, const grove::NullOpt&) {
  return true;
}

inline bool operator!=(const grove::NullOpt&, const grove::NullOpt&) {
  return false;
}

template <typename T>
inline bool operator==(const grove::Optional<T>& lhs, const grove::NullOpt&) {
  return !lhs.has_value();
}

template <typename T>
inline bool operator!=(const grove::Optional<T>& lhs, const grove::NullOpt& rhs) {
  return !(lhs == rhs);
}

template <typename T>
inline bool operator==(const grove::NullOpt&, const grove::Optional<T>& rhs) {
  return !rhs.has_value();
}

template <typename T>
inline bool operator!=(const grove::NullOpt& lhs, const grove::Optional<T>& rhs) {
  return !(lhs == rhs);
}

template <typename T>
inline bool operator==(const grove::Optional<T>& lhs, const grove::Optional<T>& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (!lhs.has_value()) {
    return true;
  }

  return lhs.value() == rhs.value();
}

namespace grove {
  template <typename T>
  inline bool optional_less(const Optional<T>& lhs, const Optional<T>& rhs) {
    bool lhs_has_val = lhs.has_value();
    bool rhs_has_val = rhs.has_value();

    //  Empty is less than full.
    if (!lhs_has_val && rhs_has_val) {
      return true;
    } else if (rhs_has_val && !lhs_has_val) {
      return false;
    }

    //  Equal -- both empty.
    if (!lhs_has_val) {
      return false;
    }

    return lhs.value() < rhs.value();
  }
}