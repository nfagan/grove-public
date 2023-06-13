#pragma once

#include "types.hpp"

namespace grove {

template <typename Lifecycle>
struct GLResource {
  GLResource() noexcept : handle(0), is_created(false) {
    //
  }

  ~GLResource() noexcept {
    dispose();
  }

  GLResource(const GLResource& other) = delete;
  GLResource& operator=(const GLResource& other) = delete;

  GLResource(GLResource&& other) noexcept : handle(other.handle), is_created(other.is_created) {
    other.handle = 0;
    other.is_created = false;
  }

  GLResource& operator=(GLResource&& other) noexcept {
    if (this != &other) {
      dispose();

      std::swap(handle, other.handle);
      std::swap(is_created, other.is_created);
    }

    return *this;
  }

  void create() {
    if (is_created) {
      dispose();
    }

    Lifecycle::create(1, &handle);
    is_created = true;
  }

  void dispose() noexcept {
    if (is_created) {
      Lifecycle::dispose(1, &handle);
      handle = 0;
      is_created = false;
    }
  }

  unsigned int handle;
  bool is_created;
};

}