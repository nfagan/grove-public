#pragma once

#include "GLResource.hpp"
#include "grove/common/common.hpp"

namespace grove {

namespace gl {
  struct GLRenderbufferLifecycle {
    static void create(int number, unsigned int* ids);
    static void dispose(int number, unsigned int* ids);
  };
}

class GLRenderbuffer {
  friend class GLRenderContext;
public:
  void create();
  void dispose();
  bool is_valid() const;

  void depth_storage(int width, int height, int samples = 0) const;
  void storage(unsigned int internal_format, int width, int height, int samples = 0) const;

  unsigned int get_instance_handle() const;

private:
  void bind() const;
  void unbind() const;

private:
  GLResource<gl::GLRenderbufferLifecycle> instance;
};

}