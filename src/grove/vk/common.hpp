#pragma once

#include <vulkan/vulkan.h>
#include <utility>
#include <cassert>

#define GROVE_NONCOPYABLE(class_name)                       \
  class_name(const class_name& other) = delete;             \
  class_name& operator=(const class_name& other) = delete;

#define GROVE_NONMOVEABLE(class_name)                       \
  class_name(class_name&& other) = delete;                  \
  class_name& operator=(class_name&& other) = delete;

#define GROVE_ASSERT assert
#define GROVE_VK_ALLOC nullptr
#define GROVE_VK_CHECK(stmt)       \
{                                  \
  const VkResult res = (stmt);     \
  (void) res;                      \
  if (res != VK_SUCCESS) {         \
    GROVE_ASSERT(false);           \
  }                                \
}

#define GROVE_VK_CHECK_ERR(stmt) \
{                                \
  vk::Error err = (stmt);        \
  if (err) {                     \
    GROVE_ASSERT(false);         \
  }                              \
}

#define GROVE_VK_TRY_ERR(stmt) \
  if (auto err = (stmt)) {     \
    return err;                \
  }

namespace grove::vk {

template <typename T>
struct Result {
  Result() = default;
  Result(VkResult err, const char* msg) : status{err}, message{msg} {}
  Result(T&& v) : value{std::move(v)}, status{VK_SUCCESS} {}
  Result(const T& v) : value{v}, status{VK_SUCCESS} {}

  inline operator bool() const {
    return status == VK_SUCCESS;
  }

  T value;
  VkResult status{VK_ERROR_UNKNOWN};
  const char* message{""};
};

struct Error {
  Error() = default;
  Error(VkResult res, const char* msg) : result{res}, message{msg} {}

  inline operator bool() const {
    return result != VK_SUCCESS;
  }

  VkResult result{VK_SUCCESS};
  const char* message{""};
};

template <typename Dst, typename Src>
Result<Dst> error_cast(const Result<Src>& src) {
  return Result<Dst>{src.status, src.message};
}

template <typename Dst>
Result<Dst> error_cast(const Error& src) {
  return Result<Dst>{src.result, src.message};
}

}