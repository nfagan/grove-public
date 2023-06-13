#pragma once

#include "grove/common/Optional.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace grove::obj {

enum class AttributeType {
  Position,
  Normal,
  TexCoord
};

struct VertexData {
  std::vector<float> packed_data;
  std::vector<int> attribute_sizes;
  std::vector<AttributeType> attribute_types;

  Optional<int> find_attribute(AttributeType type) const;

  int num_vertices() const;
  int vertex_stride() const;
};

struct MaterialDescriptor {
  std::string name;
  std::string ambient_texture_name;
  std::string diffuse_texture_name;
  std::string specular_texture_name;
  std::string bump_texture_name;
};

struct Data {
  VertexData vertex_data;
  std::vector<std::size_t> material_indices;
  std::vector<std::size_t> face_indices;
  std::vector<MaterialDescriptor> materials;
};

struct Params {
  //
};

Data load_complete(const char* file_path, const char* material_directory,
                   bool* success, const Params& params = {});

VertexData load_simple(const char* file_path, const char* material_directory, bool* success);

}