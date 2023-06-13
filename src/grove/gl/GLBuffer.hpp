#pragma once

#include "types.hpp"
#include "GLResource.hpp"
#include "grove/common/common.hpp"
#include <vector>

namespace grove {

namespace gl {
  struct BufferLifecycle {
    static void create(int num, unsigned int* ids);
    static void dispose(int num, unsigned int* ids);
  };
}

class GLBuffer {
public:
  explicit GLBuffer(BufferType buffer_type);
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(GLBuffer)
  
  void create();
  void dispose();

  bool is_valid() const;
  
  void fill(const void* data, size_t num_bytes, DrawType draw_type = DrawType::Static) const;
  void refill(const void* data, size_t num_bytes, size_t byte_offset = 0) const;
  
  template <typename T>
  void fill(const std::vector<T>& data, DrawType draw_type = DrawType::Static) const {
    fill(data.data(), data.size() * sizeof(T), draw_type);
  }
  
  template <typename T>
  void refill(const std::vector<T>& data, size_t byte_offset = 0) const {
    refill(data.data(), data.size() * sizeof(T), byte_offset);
  }

  void bind_fill(const void* data, size_t num_bytes, DrawType draw_type = DrawType::Static) const {
    bind();
    fill(data, num_bytes, draw_type);
  }

  template <typename T>
  void bind_fill(const std::vector<T>& data, DrawType draw_type = DrawType::Static) const {
    bind();
    fill(data, draw_type);
  }

  void bind() const;
  void unbind() const;
  
  unsigned int get_instance_handle() const;

protected:
  ~GLBuffer() = default;

private:
  BufferType type;
  GLResource<gl::BufferLifecycle> instance;
};

class Ebo : public GLBuffer {
public:
  Ebo() : GLBuffer(BufferType::Element) {
    //
  }
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(Ebo)
};

class Tbo : public GLBuffer {
public:
  Tbo() : GLBuffer(BufferType::Texture) {
    //
  }
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(Tbo)
};

class Vbo : public GLBuffer {
public:
  Vbo() : GLBuffer(BufferType::Array) {
    //
  }
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(Vbo)
};

class Ssbo : public GLBuffer {
public:
  Ssbo() : GLBuffer(BufferType::ShaderStorage) {
    //
  }
  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(Ssbo)

  void bind_base(int binding);
};

class GLDrawIndirectBuffer : public GLBuffer {
public:
  GLDrawIndirectBuffer() : GLBuffer(BufferType::DrawIndirect) {
    //
  }

  GROVE_DEFAULT_MOVE_CTOR_AND_ASSIGNMENT_NOEXCEPT(GLDrawIndirectBuffer)
};

}