#include "Program.hpp"
#include "Shader.hpp"
#include "GLTexture2.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/config.hpp"
#include <glad/glad.h>
#include <cassert>

GROVE_NAMESPACE_BEGIN

//  @Cleanup: Maybe play with warnings to avoid having to redeclare these for debug vs. release.
#ifdef GROVE_DEBUG
void gl::ProgramLifecycle::create(int num, unsigned int* id) {
  assert(num == 1);
  *id = glCreateProgram();
}

void gl::ProgramLifecycle::dispose(int num, unsigned int* id) {
  assert(num == 1);
  glDeleteProgram(*id);
}
#else
void gl::ProgramLifecycle::create(int, unsigned int* id) {
  *id = glCreateProgram();
}

void gl::ProgramLifecycle::dispose(int, unsigned int* id) {
  glDeleteProgram(*id);
}
#endif

Program make_compute_program(const char* source, bool* success) {
  Program program;
  program.create();
  Shader shaders[1] = {Shader::compute(source)};
  *success = program.attach_link_dispose_shaders(shaders, 1);
  if (!*success) {
    program.dispose();
  }
  return program;
}

Program make_program(const char* vertex_source, const char* fragment_source, bool* success) {
  Program program;
  program.create();
  
  Shader shaders[2] = {Shader::vertex(vertex_source), Shader::fragment(fragment_source)};
  *success = program.attach_link_dispose_shaders(shaders, 2);
  
  if (!*success) {
    program.dispose();
  }
  
  return program;
}

bool Program::is_valid() const {
  return instance.is_created;
}

void Program::create() {
  instance.create();
}

void Program::dispose() {
  instance.dispose();
}

bool Program::attach(const grove::Shader& shader) const {
  if (!check_created()) {
    return false;
  }
  
  if (!shader.is_valid()) {
    GROVE_LOG_ERROR("Attempt to attach an invalid shader.");
    return false;
  }
  
  glAttachShader(instance.handle, shader.shader);
  return true;
}

void Program::bind() const {
  assert(is_valid() && "Invalid program.");
  glUseProgram(instance.handle);
}

void Program::unbind() const {
  assert(is_valid() && "Invalid program.");
  glUseProgram(0);
}

unsigned int Program::get_instance_handle() const {
  return instance.handle;
}

bool Program::attach_link_dispose_shaders(grove::Shader* shaders,
                                          int num_shaders,
                                          bool dispose_on_error) const {
  if (!check_created()) {
    return false;
  }
  
  bool success = true;
  
  for (int i = 0; i < num_shaders; i++) {
    success = attach(shaders[i]);

    if (!success) {
      break;
    }
  }
  
  if (success) {
    success = link();
  }
  
  if (success || dispose_on_error) {
    for (int i = 0; i < num_shaders; i++) {
      shaders[i].dispose();
    }
  }
  
  return success;
}

bool Program::link() const {
  if (!check_created()) {
    return false;
  }
  
  glLinkProgram(instance.handle);
  
  int success = 0;
  glGetProgramiv(instance.handle, GL_LINK_STATUS, &success);
  
  if (!success) {
    char info_log[4096];
    glGetProgramInfoLog(instance.handle, 4096, nullptr, info_log);
    GROVE_LOG_ERROR(info_log);
    return false;
  }
  
  return true;
}

std::vector<std::string> Program::active_uniform_names() const {
  check_created();
  
  std::vector<std::string> names;
  int num_uniforms;
  glGetProgramiv(instance.handle, GL_ACTIVE_UNIFORMS, &num_uniforms);
  
  for (int i = 0; i < num_uniforms; i++) {
    GLint size;
    GLenum type;
    GLchar name[256];
    GLsizei length;
    
    glGetActiveUniform(instance.handle, (GLuint)i, 256, &length, &size, &type, name);
    
    if (length > 0) {
      name[length] = '\0';
      std::string name_str(name);
      names.push_back(std::move(name_str));
    } else {
      GROVE_LOG_WARNING("Program: active_uniform_names: Active uniform was empty.");
    }
  }
  
  return names;
}

int Program::uniform_location(const char* name) const {
  return glGetUniformLocation(instance.handle, name);
}

void Program::set_float(int location, float value) const {
  glUniform1f(location, value);
}

void Program::set_int(int location, int value) const {
  glUniform1i(location, value);
}

void Program::set_float2(int location, float x, float y) const {
  glUniform2f(location, x, y);
}

void Program::set_float3(int location, float x, float y, float z) const {
  glUniform3f(location, x, y, z);
}

void Program::set_float4(int location, float x, float y, float z, float w) const {
  glUniform4f(location, x, y, z, w);
}

void Program::set_vec2(int location, const Vec2f& vec) const {
  set_float2(location, vec.x, vec.y);
}

void Program::set_vec3(int location, const Vec3f& vec) const {
  set_float3(location, vec.x, vec.y, vec.z);
}

void Program::set_vec4(int location, const Vec4f& vec) const {
  set_float4(location, vec.x, vec.y, vec.z, vec.w);
}

void Program::set_ivec2(int location, const Vec2<int>& vec) const {
  glUniform2i(location, vec.x, vec.y);
}

void Program::set_ivec3(int location, const Vec3<int>& vec) const {
  glUniform3i(location, vec.x, vec.y, vec.z);
}

void Program::set_ivec4(int location, const Vec4<int>& vec) const {
  glUniform4i(location, vec.x, vec.y, vec.z, vec.w);
}

void Program::set_mat4(int location, const Mat4f& mat) const {
  glUniformMatrix4fv(location, 1, false, &mat.elements[0]);
}

void Program::set_texture(int location, const grove::GLTexture& texture) const {
  set_int(location, texture.get_index());
}

bool Program::check_created() const {
  if (!instance.handle) {
    GROVE_LOG_ERROR("Program not yet created.");
  }
  
  return instance.is_created;
}

GROVE_NAMESPACE_END
