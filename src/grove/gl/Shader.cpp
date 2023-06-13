#include "Shader.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <glad/glad.h>

GROVE_NAMESPACE_BEGIN

Shader::Shader() : shader(0), is_created(false) {
  //
}

bool Shader::is_valid() const {
  return is_created;
}

void Shader::dispose() {
  if (!is_created) {
    return;
  }
  
  glDeleteShader(shader);
  is_created = false;
}

bool Shader::create(grove::ShaderType type, const char* source) {
  if (is_created) {
    dispose();
  }
  
  shader = glCreateShader(gl::shader_type(type));
  
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  
  int success = 0;
  
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  
  if (!success) {
    char info_log[1024];
    glGetShaderInfoLog(shader, 1024, nullptr, info_log);
    GROVE_LOG_SEVERE_CAPTURE_META(info_log, "Shader/create");
    
    return false;
  }
  
  is_created = true;
  return true;
}

Shader Shader::vertex(const char* source) {
  Shader shader;
  shader.create(ShaderType::Vertex, source);
  return shader;
}

Shader Shader::fragment(const char* source) {
  Shader shader;
  shader.create(ShaderType::Fragment, source);
  return shader;
}

Shader Shader::compute(const char* source) {
  Shader shader;
  shader.create(ShaderType::Compute, source);
  return shader;
}

GROVE_NAMESPACE_END

