#pragma once

namespace grove {

namespace detail {
  struct LeftBase {};
  struct RightBase {};

  template <typename T>
  struct Left : LeftBase {
    template <typename U>
    constexpr Left(U&& v) : v{std::forward<U>(v)} {}
    T v;
  };

  template <typename T>
  struct Right : RightBase {
    template <typename U>
    constexpr Right(U&& v) : v{std::forward<U>(v)} {}
    T v;
  };
}

template <typename T, typename E>
class Either {
public:
  using LeftType = T;
  using RightType = E;

  constexpr Either(detail::Left<T> l) noexcept : left{std::move(l.v)}, is_left(true) {
    //
  }

  constexpr Either(detail::Right<E> r) noexcept : right{std::move(r.v)} {
    //
  }

  operator bool() const {
    return is_left;
  }

  T& get_left() {
    return left;
  }
  const T& get_left() const {
    return left;
  }

  E& get_right() {
    return right;
  }
  const E& get_right() const {
    return right;
  }

private:
  T left;
  E right;
  bool is_left{};
};

namespace either {
  template <typename T, typename U, typename V>
  Either<T, U> right(V&& v) {
    return Either<T, U>(detail::Right<U>(std::forward<V>(v)));
  }

  template <typename T, typename U, typename V>
  Either<T, U> left(V&& v) {
    return Either<T, U>(detail::Left<T>(std::forward<V>(v)));
  }

  template <typename T, typename V>
  auto make_left(V&& v) {
    using L = typename T::LeftType;
    using R = typename T::RightType;
    return Either<L, R>(detail::Left<L>(std::forward<V>(v)));
  }

  template <typename T, typename V>
  auto make_right(V&& v) {
    using L = typename T::LeftType;
    using R = typename T::RightType;
    return Either<L, R>(detail::Right<R>(std::forward<V>(v)));
  }
}

}