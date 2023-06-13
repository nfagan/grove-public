#pragma once

#include "grove/glsl/preprocess.hpp"

namespace grove::glsl {

enum class ShaderType {
  Vertex,
  Fragment,
  Compute
};

enum class OptimizationType {
  None,
  Size,
  Performance
};

struct CompileOptions {
  const char* file_name{""};
  OptimizationType optimization_type{};
  IncludeProcessInstance* include_processor{};
  PreprocessorDefinitions definitions;
};

Optional<std::vector<uint32_t>> compile_spv(std::string glsl_source,
                                            ShaderType type,
                                            const CompileOptions& options);

}