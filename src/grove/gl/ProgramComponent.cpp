#include "ProgramComponent.hpp"

namespace grove {

void ProgramComponent::gather_locations(GLRenderContext& context) {
  context.bind_program(program);
  uniforms.gather_locations(program);
}

bool ProgramComponent::is_valid() const {
  return program.is_valid();
}

void ProgramComponent::dispose() {
  program.dispose();
  uniforms.clear();
}

void ProgramComponent::bind(GLRenderContext& context) const {
  context.bind_program(program);
}

}
