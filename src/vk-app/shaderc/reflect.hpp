#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "grove/common/DynamicArray.hpp"

namespace grove::glsl {

constexpr uint32_t missing_value() {
  return ~uint32_t(0);
}

enum class BaseType {
  Unknown,
  Float,
  Int,
  UInt,
  Struct,
};

struct Type {
  bool is_array() const {
    return !array_size.empty();
  }
  bool is_vec(uint32_t m) const {
    return num_columns == 1 && m == vector_size;
  }
  bool is_vec_t(uint32_t m, BaseType t) const {
    return is_vec(m) && type == t;
  }
  bool is_mat(uint32_t m, uint32_t n) const {
    return num_columns == n && m == vector_size;
  }
  bool is_mat_t(uint32_t m, uint32_t n, BaseType t) const {
    return is_mat(m, n) && type == t;
  }

  BaseType type;
  uint32_t source;
  uint32_t bits;
  uint32_t vector_size;
  uint32_t num_columns;
  DynamicArray<uint32_t, 1> array_size;
};

struct Member {
  Type type;
  bool active;
  uint32_t index;
  uint32_t offset;
  uint32_t range;
};

struct Struct {
  std::vector<Member> members;
  uint32_t size;
};

struct StructResource {
  Struct s;
  uint32_t set;
  uint32_t binding;
  DynamicArray<uint32_t, 1> array_sizes;
};

struct ImageResource {
  uint32_t set;
  uint32_t binding;
  DynamicArray<uint32_t, 1> array_sizes;
};

struct PushConstantBuffer {
  uint32_t size;
};

struct ReflectInfo {
  std::vector<StructResource> uniform_buffers;
  std::vector<StructResource> storage_buffers;
  std::vector<PushConstantBuffer> push_constant_buffers;
  std::vector<ImageResource> sampled_images;
  std::vector<ImageResource> storage_images;
  std::vector<Struct> struct_types;
};

ReflectInfo reflect_spv(std::vector<uint32_t> spv);

const char* to_string(BaseType type);
std::string to_string(const Type& type, const char* delim = "; ");
std::string to_string(const Member& member, const char* delim = "; ");
std::string to_string(const Struct& s, const std::vector<Struct>& structs, const char* delim = "; ", int indent = 0);
std::string to_string(const StructResource& s, const std::vector<Struct>& structs, const char* delim = "; ", int indent = 0);
std::string to_string(const ImageResource& image, const char* delim = "; ");

}