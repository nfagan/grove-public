#pragma once

#include "GLResource.hpp"
#include "grove/common/common.hpp"

namespace grove {

class GLTexture2;
class GLTexture2Array;
class GLRenderbuffer;

namespace gl {
  struct FramebufferLifecycle {
    static void create(int number, unsigned int* ids);
    static void dispose(int number, unsigned int* ids);
  };
}

class GLFramebuffer {
  friend class GLRenderContext;
public:
  void create();
  void dispose();
  bool is_valid() const;
  bool is_created() const;
  bool is_complete() const;

  void attach_texture2(unsigned int attachment, unsigned int texture_target, unsigned int texture_handle) const;
  void attach_texture2(unsigned int attachment, const GLTexture2& texture) const;
  void attach_texture2_array(unsigned int attachment, const GLTexture2Array& texture, int level, int layer) const;
  void attach_renderbuffer(unsigned int attachment, const GLRenderbuffer& renderbuffer) const;
  void attach_depth_renderbuffer(const GLRenderbuffer& renderbuffer) const;
  void set_draw_buffers(int count, const unsigned int* attachments);
  void set_color_attachment_draw_buffers_range(int size) const;

  unsigned int get_instance_handle() const;

private:
  void bind() const;
  void unbind() const;

private:
  GLResource<gl::FramebufferLifecycle> instance;
};

}