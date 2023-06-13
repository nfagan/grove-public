#pragma once

#include "../shaderc/compile.hpp"
#include "../shaderc/reflect.hpp"
#include "../shaderc/vk/reflect_resource.hpp"

namespace grove::glsl {

struct VertFragCompileParams {
  PreprocessorDefinitions vert_defines;
  PreprocessorDefinitions frag_defines;
  OptimizationType optimization_type{OptimizationType::Performance};
  IncludeProcessInstance* include_processor{};
  bool process_includes{true};
};

struct VertFragBytecode {
  std::vector<uint32_t> vert_bytecode;
  std::vector<uint32_t> frag_bytecode;
};

struct VertFragReflectParams {
  vk::refl::ToVkDescriptorType to_vk_descriptor_type{nullptr};
};

struct VertFragReflectInfo {
  ReflectInfo vert;
  ReflectInfo frag;
  vk::refl::LayoutBindingsBySet descriptor_set_layout_bindings;
  vk::refl::PushConstantRanges push_constant_ranges;
};

struct VertFragProgramSource {
  std::vector<uint32_t> vert_bytecode;
  std::vector<uint32_t> frag_bytecode;
  vk::refl::LayoutBindingsBySet descriptor_set_layout_bindings;
  vk::refl::PushConstantRanges push_constant_ranges;
};

struct LoadVertFragProgramSourceParams {
  const char* vert_file{nullptr};
  const char* frag_file{nullptr};
  std::string vert_source;
  std::string frag_source;
  VertFragCompileParams compile{};
  VertFragReflectParams reflect{};
};

struct ComputeCompileParams {
  PreprocessorDefinitions defines;
  OptimizationType optimization_type{OptimizationType::Performance};
  IncludeProcessInstance* include_processor{};
  bool process_includes{true};
};

struct ComputeBytecode {
  std::vector<uint32_t> bytecode;
};

struct ComputeReflectParams {
  vk::refl::ToVkDescriptorType to_vk_descriptor_type{nullptr};
};

struct ComputeReflectInfo {
  ReflectInfo compute;
  vk::refl::LayoutBindingsBySet descriptor_set_layout_bindings;
  vk::refl::PushConstantRanges push_constant_ranges;
};

struct ComputeProgramSource {
  std::vector<uint32_t> bytecode;
  vk::refl::LayoutBindingsBySet descriptor_set_layout_bindings;
  vk::refl::PushConstantRanges push_constant_ranges;
};

struct LoadComputeProgramSourceParams {
  const char* file{nullptr};
  std::string source;
  ComputeCompileParams compile{};
  ComputeReflectParams reflect{};
};

Optional<VertFragBytecode> compile_vert_frag_spv(const std::string& vert_source,
                                                 const std::string& frag_source,
                                                 const VertFragCompileParams& params = {});
Optional<ComputeBytecode> compile_compute_spv(const std::string& source,
                                              const ComputeCompileParams& params = {});

Optional<VertFragBytecode> compile_vert_frag_spv_from_file(const char* vert_file,
                                                           const char* frag_file,
                                                           const VertFragCompileParams& params = {});

Optional<std::vector<uint32_t>> default_compile_spv(std::string source,
                                                    const char* name,
                                                    ShaderType type,
                                                    const PreprocessorDefinitions& defs);

Optional<std::vector<uint32_t>>
default_compile_spv_from_file(const char* name,
                              ShaderType type,
                              const glsl::PreprocessorDefinitions& defs = {});

Optional<VertFragReflectInfo> reflect_vert_frag_spv(const std::vector<uint32_t>& vert_spv,
                                                    const std::vector<uint32_t>& frag_spv,
                                                    const VertFragReflectParams& params = {});
Optional<VertFragReflectInfo> reflect_vert_frag_spv(const VertFragBytecode& source,
                                                    const VertFragReflectParams& params = {});
Optional<ComputeReflectInfo> reflect_compute_spv(const std::vector<uint32_t>& spv,
                                                 const ComputeReflectParams& params = {});

Optional<VertFragProgramSource> make_vert_frag_program_source(const LoadVertFragProgramSourceParams& params);
Optional<ComputeProgramSource> make_compute_program_source(const LoadComputeProgramSourceParams& params);

void set_default_shader_directory(std::string dir);

}