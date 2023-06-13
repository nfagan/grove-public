#include "reflect.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <spirv_cross/spirv_glsl.hpp>

GROVE_NAMESPACE_BEGIN

namespace {

struct Context {
  std::unordered_map<uint32_t, uint32_t> id_mapping;
  std::vector<glsl::Struct> structs;
};

[[maybe_unused]] constexpr const char* logging_id() {
  return "shaderc/reflect";
}

std::string spaces(int n) {
  return std::string(n, ' ');
}

uint32_t get_decoration_or_missing(const spirv_cross::CompilerGLSL& glsl,
                                   spirv_cross::ID id,
                                   spv::Decoration decoration) {
  if (glsl.has_decoration(id, decoration)) {
    return glsl.get_decoration(id, decoration);
  } else {
    return glsl::missing_value();
  }
}

glsl::BaseType to_base_type(spirv_cross::SPIRType::BaseType type) {
  using BaseT = spirv_cross::SPIRType::BaseType;
  switch (type) {
    case BaseT::Float:
      return glsl::BaseType::Float;
    case BaseT::Int:
      return glsl::BaseType::Int;
    case BaseT::UInt:
      return glsl::BaseType::UInt;
    case BaseT::Struct:
      return glsl::BaseType::Struct;
    default:
      return glsl::BaseType::Unknown;
  }
}

glsl::Type to_type(const spirv_cross::SPIRType& type) {
  glsl::Type result{};
  result.type = to_base_type(type.basetype);
  result.bits = type.width;
  result.vector_size = type.vecsize;
  result.num_columns = type.columns;
  for (auto& s : type.array) {
    result.array_size.push_back(s);
  }
  return result;
}

glsl::ImageResource to_image(const spirv_cross::CompilerGLSL& glsl,
                             const spirv_cross::Resource& image) {
  glsl::ImageResource result{};
  const auto& type = glsl.get_type_from_variable(image.id);
  if (type.array.size() > 1) {
    GROVE_LOG_ERROR_CAPTURE_META(
      "Multidimensional image resource arrays not supported.", logging_id());
    assert(false);
  }
  for (auto& s : type.array) {
    result.array_sizes.push_back(s);
  }
  result.set = get_decoration_or_missing(glsl, image.id, spv::DecorationDescriptorSet);
  result.binding = get_decoration_or_missing(glsl, image.id, spv::DecorationBinding);
  return result;
}

glsl::Type parse_type(const spirv_cross::CompilerGLSL& glsl,
                      const spirv_cross::SPIRType& type,
                      uint32_t type_id,
                      Context& context) {
  auto res = to_type(type);
  if (type.basetype == spirv_cross::SPIRType::Struct) {
    if (auto it = context.id_mapping.find(type_id); it != context.id_mapping.end()) {
      res.source = it->second;
    } else {
      const auto id = uint32_t(context.structs.size());
      context.structs.push_back({});
      context.id_mapping[type_id] = id;
      res.source = id;
      for (auto& mem : type.member_types) {
        glsl::Member member{};
        member.type = parse_type(glsl, glsl.get_type(mem), uint32_t(mem), context);
        context.structs[id].members.push_back(member);
      }
      auto& s = context.structs[id];
      s.size = uint32_t(glsl.get_declared_struct_size(type));
    }
  }
  return res;
}

glsl::PushConstantBuffer to_push_constant_buffer(const spirv_cross::CompilerGLSL& glsl,
                                                 const spirv_cross::Resource& pc) {
  const auto& type = glsl.get_type_from_variable(pc.id);
  glsl::PushConstantBuffer buff{};
  buff.size = uint32_t(glsl.get_declared_struct_size(type));
  return buff;
}

glsl::StructResource to_struct_resource(const spirv_cross::CompilerGLSL& glsl,
                                        const spirv_cross::Resource& resource,
                                        Context& context) {
  auto ranges = glsl.get_active_buffer_ranges(resource.id);
  const auto& type = glsl.get_type_from_variable(resource.id);

  glsl::StructResource result{};
  result.set = get_decoration_or_missing(glsl, resource.id, spv::DecorationDescriptorSet);
  result.binding = get_decoration_or_missing(glsl, resource.id, spv::DecorationBinding);
  result.s.size = uint32_t(glsl.get_declared_struct_size(type));

  if (type.array.size() > 1) {
    GROVE_LOG_ERROR_CAPTURE_META(
      "Multidimensional struct resource arrays not supported.", logging_id());
    assert(false);
  }
  for (auto& s : type.array) {
    result.array_sizes.push_back(s);
  }

  std::unordered_map<uint32_t, spirv_cross::BufferRange> active_ranges;
  for (auto& range : ranges) {
    assert(active_ranges.count(range.index) == 0);
    active_ranges[range.index] = range;
  }

  uint32_t ind{};
  for (auto& mem : type.member_types) {
    glsl::Member member{};
    member.type = parse_type(glsl, glsl.get_type(mem), uint32_t(mem), context);
    member.index = ind;
    if (active_ranges.count(ind)) {
      auto& rng = active_ranges.at(ind);
      member.active = true;
      member.offset = uint32_t(rng.offset);
      member.range = uint32_t(rng.range);
    }
    result.s.members.push_back(member);
    ind++;
  }

  return result;
}

} //  anon

glsl::ReflectInfo glsl::reflect_spv(std::vector<uint32_t> spv) {
  Context context{};
  spirv_cross::CompilerGLSL glsl{std::move(spv)};
  spirv_cross::ShaderResources resources = glsl.get_shader_resources();

  glsl::ReflectInfo result;
  for (auto& buff : resources.uniform_buffers) {
    result.uniform_buffers.push_back(to_struct_resource(glsl, buff, context));
  }
  for (auto& buff : resources.storage_buffers) {
    result.storage_buffers.push_back(to_struct_resource(glsl, buff, context));
  }
  for (auto& im : resources.sampled_images) {
    result.sampled_images.push_back(to_image(glsl, im));
  }
  for (auto& im : resources.storage_images) {
    result.storage_images.push_back(to_image(glsl, im));
  }
  for (auto& pc : resources.push_constant_buffers) {
    result.push_constant_buffers.push_back(to_push_constant_buffer(glsl, pc));
  }

  result.struct_types = std::move(context.structs);
  return result;
}

const char* glsl::to_string(BaseType type) {
  switch (type) {
    case BaseType::Unknown:
      return "Unknown";
    case BaseType::Float:
      return "Float";
    case BaseType::Int:
      return "Int";
    case BaseType::UInt:
      return "UInt";
    case BaseType::Struct:
      return "Struct";
    default:
      assert(false);
      return "";
  }
}

std::string glsl::to_string(const Type& type, const char* delim) {
  std::string res{"type: "};
  res += to_string(type.type);
  res += delim;
  res += "bits: ";
  res += std::to_string(type.bits);
  res += delim;
  res += "vector_size: ";
  res += std::to_string(type.vector_size);
  res += delim;
  res += "num_columns: ";
  res += std::to_string(type.num_columns);
  res += delim;
  if (!type.array_size.empty()) {
    res += "array_size: ";
    for (auto& s : type.array_size) {
      res += "[";
      res += std::to_string(s);
      res += "]";
    }
  }
  return res;
}

std::string glsl::to_string(const Member& member, const char* delim) {
  auto res = to_string(member.type, delim);
  res += delim;
  res += "active: ";
  res += std::to_string(member.active);
  res += delim;
  res += "index: ";
  res += std::to_string(member.index);
  res += delim;
  res += "offset: ";
  res += std::to_string(member.offset);
  res += delim;
  res += "range: ";
  res += std::to_string(member.range);
  return res;
}

std::string glsl::to_string(const StructResource& resource,
                            const std::vector<Struct>& structs,
                            const char* delim,
                            int indent) {
  std::string res;
  res += "Resource";
  res += delim;
  res += "set: ";
  res += std::to_string(resource.set);
  res += delim;
  res += "binding: ";
  res += std::to_string(resource.binding);
  res += delim;
  return res + to_string(resource.s, structs, delim, indent);
}

std::string glsl::to_string(const Struct& s,
                            const std::vector<Struct>& structs,
                            const char* delim,
                            int indent) {
  std::string res;
  res += "Struct: ";
  res += delim;
  res += "size: ";
  res += std::to_string(s.size);
  for (auto& mem : s.members) {
    res += "\n";
    res += spaces(indent);
    res += to_string(mem, delim);
    if (mem.type.type == BaseType::Struct) {
      res += to_string(structs[mem.type.source], structs, delim, indent + 2);
    }
  }
  return res;
}

std::string glsl::to_string(const ImageResource& image, const char* delim) {
  std::string res;
  res += "Image";
  res += delim;
  res += "set: ";
  res += std::to_string(image.set);
  res += delim;
  res += "binding: ";
  res += std::to_string(image.binding);
  return res;
}

GROVE_NAMESPACE_END