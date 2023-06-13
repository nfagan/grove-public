#pragma once

#define GROVE_NAMESPACE_BEGIN namespace grove {
#define GROVE_NAMESPACE_END }

#define GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(class_name) \
  class_name(const class_name& other) = delete; \
  class_name& operator=(const class_name& other) = delete;

#define GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(class_name) \
  class_name(class_name&& other) = default; \
  class_name& operator=(class_name&& other) = default;

#define GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(class_name) \
  class_name(class_name&& other) noexcept = default; \
  class_name& operator=(class_name&& other) noexcept = default;

#ifdef _MSC_VER
#define MAYBE_NOEXCEPT
#else
#define MAYBE_NOEXCEPT noexcept
#endif