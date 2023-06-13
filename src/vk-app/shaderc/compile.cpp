#include "compile.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <shaderc/shaderc.hpp>

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "shaderc";
}

shaderc_shader_kind to_shaderc_shader_kind(glsl::ShaderType type) {
  switch (type) {
    case glsl::ShaderType::Vertex:
      return shaderc_vertex_shader;
    case glsl::ShaderType::Fragment:
      return shaderc_fragment_shader;
    case glsl::ShaderType::Compute:
      return shaderc_compute_shader;
    default:
      assert(false);
      return shaderc_vertex_shader;
  }
}

shaderc_optimization_level to_shaderc_optimization_level(glsl::OptimizationType type) {
  switch (type) {
    case glsl::OptimizationType::None:
      return shaderc_optimization_level_zero;
    case glsl::OptimizationType::Performance:
      return shaderc_optimization_level_performance;
    case glsl::OptimizationType::Size:
      return shaderc_optimization_level_size;
    default:
      assert(false);
      return shaderc_optimization_level_zero;
  }
}

Optional<std::vector<uint32_t>> glsl_to_spv(const std::string& glsl_source,
                                            shaderc_shader_kind kind,
                                            shaderc_optimization_level optimization_level,
                                            const char* file_name) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(optimization_level);

  shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
    glsl_source, kind, file_name, options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
#if GROVE_LOGGING_ENABLED
    auto msg = module.GetErrorMessage();
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
#endif
    return NullOpt{};
  }

  std::vector<uint32_t> res{module.cbegin(), module.cend()};
  return Optional<std::vector<uint32_t>>(std::move(res));
}

void maybe_log_errors(const glsl::IncludeProcessInstance& inst) {
#if GROVE_LOGGING_ENABLED
  for (auto& err : inst.result.errors) {
    GROVE_LOG_ERROR_CAPTURE_META(err.message.c_str(), logging_id());
  }
#else
  (void) inst;
#endif
}

} //  anon

Optional<std::vector<uint32_t>> glsl::compile_spv(std::string source,
                                                  ShaderType type,
                                                  const CompileOptions& options) {
  if (options.include_processor) {
    if (auto src = glsl::fill_in_includes(source, *options.include_processor)) {
      source = std::move(src.value());
    } else {
      maybe_log_errors(*options.include_processor);
      return NullOpt{};
    }
  }
  if (!options.definitions.empty()) {
    source = glsl::set_preprocessor_definitions(source, options.definitions);
  }
  return glsl_to_spv(
    source,
    to_shaderc_shader_kind(type),
    to_shaderc_optimization_level(options.optimization_type),
    options.file_name);
}

GROVE_NAMESPACE_END
