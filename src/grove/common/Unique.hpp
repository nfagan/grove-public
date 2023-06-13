#pragma once

#include <functional>

namespace grove {

template <typename T>
class Unique {
  using Dtor = std::function<void(T*)>;
public:
  Unique() = default;
  Unique(T&& data, Dtor&& dtor) :
    data{std::move(data)},
    dtor{std::move(dtor)} {
    //
  }
  Unique(const Unique& other) = delete;
  Unique& operator=(const Unique& other) = delete;
  Unique(Unique&& other) noexcept :
    data{std::move(other.data)},
    dtor{std::move(other.dtor)} {
    //
    other.data = T{};
    other.dtor = nullptr;
  }
  Unique& operator=(Unique&& other) noexcept {
    Unique tmp{std::move(other)};
    swap(*this, tmp);
    return *this;
  }
  ~Unique() {
    if (dtor) {
      dtor(&data);
      data = T{};
      dtor = nullptr;
    }
  }

  T& get() {
    return data;
  }
  const T& get() const {
    return data;
  }
  bool has_value() const {
    return dtor != nullptr;
  }

  friend inline void swap(Unique& a, Unique& b) noexcept {
    using std::swap;
    swap(a.data, b.data);
    swap(a.dtor, b.dtor);
  }
private:
  T data{};
  Dtor dtor{nullptr};
};

}