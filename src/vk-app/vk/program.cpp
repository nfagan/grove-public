#include "program.hpp"
#include "grove/common/common.hpp"
#include "grove/common/fs.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

std::string default_shader_directory;

glsl::IncludeProcessInstance make_default_include_processor() {
  return glsl::IncludeProcessInstance{default_shader_directory};
}

std::string shader_full_path(const char* file) {
  return default_shader_directory + "/" + file;
}

} //  anon

Optional<std::vector<uint32_t>>
glsl::default_compile_spv(std::string source, const char* name,
                          ShaderType type, const PreprocessorDefinitions& defs) {
  IncludeProcessInstance include_processor{default_shader_directory};
  CompileOptions options{};
  options.include_processor = &include_processor;
  options.file_name = name;
  options.optimization_type = glsl::OptimizationType::Performance;
  options.definitions = defs;
  return compile_spv(std::move(source), type, options);
}

Optional<std::vector<uint32_t>>
glsl::default_compile_spv_from_file(const char* name, ShaderType type,
                                    const glsl::PreprocessorDefinitions& defs) {
  bool success{};
  auto src_p = shader_full_path(name);
  auto source = read_text_file(src_p.c_str(), &success);
  if (!success) {
    return NullOpt{};
  } else {
    return default_compile_spv(std::move(source), name, type, defs);
  }
}

Optional<glsl::VertFragBytecode>
glsl::compile_vert_frag_spv(const std::string& vert_source,
                            const std::string& frag_source,
                            const VertFragCompileParams& params) {
  auto default_include_processor = make_default_include_processor();
  glsl::CompileOptions options{};
  options.optimization_type = params.optimization_type;
  if (params.process_includes) {
    options.include_processor = params.include_processor ?
      params.include_processor : &default_include_processor;
  }
  options.definitions = params.vert_defines;
  //  Vert
  auto vert_spv_res = glsl::compile_spv(vert_source, glsl::ShaderType::Vertex, options);
  if (!vert_spv_res) {
    return NullOpt{};
  }
  //  Frag
  options.include_processor->result.reset();
  options.definitions = params.frag_defines;
  auto frag_spv_res = glsl::compile_spv(frag_source, glsl::ShaderType::Fragment, options);
  if (!frag_spv_res) {
    return NullOpt{};
  }

  glsl::VertFragBytecode result;
  result.vert_bytecode = std::move(vert_spv_res.value());
  result.frag_bytecode = std::move(frag_spv_res.value());
  return Optional<glsl::VertFragBytecode>(std::move(result));
}

Optional<glsl::ComputeBytecode> glsl::compile_compute_spv(const std::string& source,
                                                          const ComputeCompileParams& params) {
  auto default_include_processor = make_default_include_processor();

  glsl::CompileOptions options{};
  options.optimization_type = params.optimization_type;
  if (params.process_includes) {
    options.include_processor = params.include_processor ?
      params.include_processor : &default_include_processor;
  }

  options.definitions = params.defines;
  auto spv_res = glsl::compile_spv(source, glsl::ShaderType::Compute, options);
  if (!spv_res) {
    return NullOpt{};
  }

  glsl::ComputeBytecode result;
  result.bytecode = std::move(spv_res.value());
  return Optional<glsl::ComputeBytecode>(std::move(result));
}

Optional<glsl::VertFragBytecode>
glsl::compile_vert_frag_spv_from_file(const char* vert_file,
                                      const char* frag_file,
                                      const VertFragCompileParams& params) {
  std::string vert_source;
  std::string frag_source;
  {
    bool success{};
    auto src_p = shader_full_path(vert_file);
    vert_source = read_text_file(src_p.c_str(), &success);
    if (!success) {
      return NullOpt{};
    }
  }
  {
    bool success{};
    auto src_p = shader_full_path(frag_file);
    frag_source = read_text_file(src_p.c_str(), &success);
    if (!success) {
      return NullOpt{};
    }
  }
  return compile_vert_frag_spv(vert_source, frag_source, params);
}

Optional<glsl::VertFragReflectInfo>
glsl::reflect_vert_frag_spv(const std::vector<uint32_t>& vert_spv,
                            const std::vector<uint32_t>& frag_spv,
                            const VertFragReflectParams& params) {
  using namespace vk::refl;

  auto vert_reflect = glsl::reflect_spv(vert_spv);
  auto frag_reflect = glsl::reflect_spv(frag_spv);
  auto refl_res = reflect_vert_frag_descriptor_set_layouts(vert_reflect, frag_reflect);
  if (!refl_res) {
    return NullOpt{};
  }

  auto to_descr_type = params.to_vk_descriptor_type ?
    params.to_vk_descriptor_type : identity_descriptor_type;
  auto layout_bindings = to_vk_descriptor_set_layout_bindings(refl_res.value(), to_descr_type);

  auto push_const_ranges = to_vk_push_constant_ranges(
    reflect_vert_frag_push_constant_ranges(
      vert_reflect.push_constant_buffers, frag_reflect.push_constant_buffers));

  glsl::VertFragReflectInfo result;
  result.vert = std::move(vert_reflect);
  result.frag = std::move(frag_reflect);
  result.descriptor_set_layout_bindings = std::move(layout_bindings);
  result.push_constant_ranges = std::move(push_const_ranges);
  return Optional<glsl::VertFragReflectInfo>(std::move(result));
}

Optional<glsl::VertFragReflectInfo>
glsl::reflect_vert_frag_spv(const VertFragBytecode& source,
                            const VertFragReflectParams& params) {
  return reflect_vert_frag_spv(source.vert_bytecode, source.frag_bytecode, params);
}

Optional<glsl::ComputeReflectInfo> glsl::reflect_compute_spv(const std::vector<uint32_t>& spv,
                                                             const ComputeReflectParams& params) {
  using namespace vk::refl;

  auto reflect = glsl::reflect_spv(spv);
  auto refl_res = reflect_compute_descriptor_set_layouts(reflect);
  if (!refl_res) {
    return NullOpt{};
  }

  auto to_descr_type = params.to_vk_descriptor_type ?
    params.to_vk_descriptor_type : identity_descriptor_type;
  auto layout_bindings = to_vk_descriptor_set_layout_bindings(refl_res.value(), to_descr_type);

  auto push_const_ranges = to_vk_push_constant_ranges(
    reflect_compute_push_constant_ranges(reflect.push_constant_buffers));

  glsl::ComputeReflectInfo result;
  result.compute = std::move(reflect);
  result.descriptor_set_layout_bindings = std::move(layout_bindings);
  result.push_constant_ranges = std::move(push_const_ranges);
  return Optional<glsl::ComputeReflectInfo>(std::move(result));
}

Optional<glsl::VertFragProgramSource>
glsl::make_vert_frag_program_source(const LoadVertFragProgramSourceParams& params) {
  std::string maybe_loaded_vert_source;
  std::string maybe_loaded_frag_source;
  const std::string* read_vert_source = &params.vert_source;
  const std::string* read_frag_source = &params.frag_source;
  if (params.vert_file) {
    auto vert_p = shader_full_path(params.vert_file);
    if (auto vert_source = read_text_file(vert_p.c_str())) {
      maybe_loaded_vert_source = std::move(vert_source.value());
      read_vert_source = &maybe_loaded_vert_source;
    } else {
      return NullOpt{};
    }
  }
  if (params.frag_file) {
    auto frag_p = shader_full_path(params.frag_file);
    if (auto frag_source = read_text_file(frag_p.c_str())) {
      maybe_loaded_frag_source = std::move(frag_source.value());
      read_frag_source = &maybe_loaded_frag_source;
    } else {
      return NullOpt{};
    }
  }
  auto compile_res = glsl::compile_vert_frag_spv(
    *read_vert_source, *read_frag_source, params.compile);
  if (!compile_res) {
    return NullOpt{};
  }
  auto reflect_res = glsl::reflect_vert_frag_spv(compile_res.value(), params.reflect);
  if (!reflect_res) {
    return NullOpt{};
  }
  return Optional<glsl::VertFragProgramSource>{glsl::VertFragProgramSource{
    std::move(compile_res.value().vert_bytecode),
    std::move(compile_res.value().frag_bytecode),
    std::move(reflect_res.value().descriptor_set_layout_bindings),
    std::move(reflect_res.value().push_constant_ranges)
  }};
}

Optional<glsl::ComputeProgramSource>
glsl::make_compute_program_source(const grove::glsl::LoadComputeProgramSourceParams& params) {
  std::string maybe_loaded_source;
  const std::string* read_source = &params.source;
  if (params.file) {
    auto file_p = shader_full_path(params.file);
    if (auto source = read_text_file(file_p.c_str())) {
      maybe_loaded_source = std::move(source.value());
      read_source = &maybe_loaded_source;
    } else {
      return NullOpt{};
    }
  }

  auto compile_res = glsl::compile_compute_spv(*read_source, params.compile);
  if (!compile_res) {
    return NullOpt{};
  }

  auto reflect_res = glsl::reflect_compute_spv(compile_res.value().bytecode, params.reflect);
  if (!reflect_res) {
    return NullOpt{};
  }

  return Optional<glsl::ComputeProgramSource>{glsl::ComputeProgramSource{
    std::move(compile_res.value().bytecode),
    std::move(reflect_res.value().descriptor_set_layout_bindings),
    std::move(reflect_res.value().push_constant_ranges)
  }};
}

void glsl::set_default_shader_directory(std::string dir) {
  default_shader_directory = std::move(dir);
}

GROVE_NAMESPACE_END
