#pragma once

#define GROVE_GLSL_VISITOR_METHOD(name, type) \
  virtual void name(type&) {} \
  virtual void name(const type&) {}

namespace grove {

namespace glsl {
  struct CompilerDirective;

  class AstVisitor {
  public:
    GROVE_GLSL_VISITOR_METHOD(compiler_directive, CompilerDirective)
  };
}

#undef GROVE_GLSL_VISITOR_METHOD

}