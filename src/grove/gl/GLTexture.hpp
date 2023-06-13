#pragma once

#include "GLResource.hpp"
#include "grove/common/common.hpp"

namespace grove {

struct TextureParameters;

namespace gl {
  struct GLTextureLifecycle {
    static void create(int num, unsigned int* ids);
    static void dispose(int num, unsigned int* ids);
  };

  void configure_texture(unsigned int target, const TextureParameters& params);
}

class GLTexture {
protected:
  explicit GLTexture(unsigned int target);
  ~GLTexture() = default;
  GROVE_DELETE_COPY_CTOR_AND_ASSIGNMENT(GLTexture)
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT(GLTexture)

public:
  void bind() const;
  void activate() const;
  void activate_bind() const;
  void create();
  void dispose();
  bool is_valid() const;

  void bind_configure(const TextureParameters& params) const;
  void configure(const TextureParameters& params) const;

  void set_index(int to);

  unsigned long get_id() const;
  int get_index() const;
  unsigned int get_instance_handle() const;

protected:
  unsigned int target;
  GLResource<gl::GLTextureLifecycle> instance;
  int index;
  unsigned long id;

  static unsigned long num_instances;
};

}