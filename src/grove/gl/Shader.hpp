#pragma once

#include "types.hpp"

namespace grove {
  class Shader;
  class Program;
}

class grove::Shader {
  friend class Program;
  
private:
  Shader();
  bool create(grove::ShaderType type, const char* source);
  
public:
  ~Shader() = default;
  
  bool is_valid() const;
  void dispose();
  
  static Shader vertex(const char* source);
  static Shader fragment(const char* source);
  static Shader compute(const char* source);
  
private:
  unsigned int shader;
  bool is_created;
};
