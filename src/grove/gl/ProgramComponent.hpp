#pragma once

#include "Program.hpp"
#include "UniformLocationMap.hpp"
#include "GLRenderContext.hpp"

namespace grove {

class GLRenderContext;

class ProgramComponent {
public:
  void gather_locations(GLRenderContext& context);
  bool is_valid() const;
  void dispose();

  template <typename T, typename U>
  void set(const U& name, const T& value) const;

  void bind(GLRenderContext& context) const;

  Program program;
  UniformLocationMap uniforms;
};

template <typename T, typename U>
void ProgramComponent::set(const U& name, const T& value) const {
  program.set<T>(uniforms.location(name), value);
}

}