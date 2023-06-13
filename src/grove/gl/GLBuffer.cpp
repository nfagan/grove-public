#include "GLBuffer.hpp"
#include "grove/common/common.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

void gl::BufferLifecycle::create(int num, unsigned int* ids) {
  glGenBuffers(num, ids);
}

void gl::BufferLifecycle::dispose(int num, unsigned int* ids) {
  glDeleteBuffers(num, ids);
}

GLBuffer::GLBuffer(BufferType buffer_type) : type(buffer_type) {
  //
}

void GLBuffer::create() {
  assert(!is_valid() && "Buffer was already created.");
  instance.create();
}

bool GLBuffer::is_valid() const {
  return instance.is_created;
}

void GLBuffer::bind() const {
  assert(is_valid() && "Invalid buffer.");
  glBindBuffer(gl::buffer_type(type), instance.handle);
}

void GLBuffer::unbind() const {
  assert(is_valid() && "Invalid buffer.");
  glBindBuffer(gl::buffer_type(type), 0);
}

void GLBuffer::fill(const void* data, size_t num_bytes, grove::DrawType draw_type) const {
  assert(is_valid() && "Invalid buffer.");
  glBufferData(gl::buffer_type(type), num_bytes, data, gl::draw_type(draw_type));
}

void GLBuffer::refill(const void* data, size_t num_bytes, size_t byte_offset) const {
  assert(is_valid() && "Invalid buffer.");
  glBufferSubData(gl::buffer_type(type), byte_offset, num_bytes, data);
}

unsigned int GLBuffer::get_instance_handle() const {
  return instance.handle;
}

void GLBuffer::dispose() {
  instance.dispose();
}

/*
 * Ssbo
 */

void Ssbo::bind_base(int binding) {
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, get_instance_handle());
}

GROVE_NAMESPACE_END
