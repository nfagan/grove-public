#include "obj.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/config.hpp"
#include <numeric>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

GROVE_NAMESPACE_BEGIN

namespace {

struct MarkedPresent {
  bool marked_has_position{};
  bool marked_has_normal{};
  bool marked_has_uv{};
};

void maybe_log(const std::string& err, const std::string& warn) {
#ifdef GROVE_DEBUG
  if (!warn.empty()) {
    GROVE_LOG_WARNING_CAPTURE_META(warn.c_str(), "obj::load_simple");
  }

  if (!err.empty()) {
    GROVE_LOG_ERROR_CAPTURE_META(err.c_str(), "obj::load_simple");
  }
#else
  (void) err;
  (void) warn;
#endif
}

void push_packed_data(const tinyobj::shape_t& shape,
                      const tinyobj::attrib_t& attrib,
                      size_t index_offset,
                      size_t fv,
                      obj::VertexData& result,
                      MarkedPresent& marked_present) {
  for (size_t i = 0; i < fv; i++) {
    const tinyobj::index_t index = shape.mesh.indices[index_offset + i];
    const bool has_position = index.vertex_index >= 0;
    const bool has_normal = index.normal_index >= 0;
    const bool has_uv = index.texcoord_index >= 0;

    if (!marked_present.marked_has_position && has_position) {
      result.attribute_sizes.push_back(3);
      result.attribute_types.push_back(obj::AttributeType::Position);
      marked_present.marked_has_position = true;
    }

    if (!marked_present.marked_has_normal && has_normal) {
      result.attribute_sizes.push_back(3);
      result.attribute_types.push_back(obj::AttributeType::Normal);
      marked_present.marked_has_normal = true;
    }

    if (!marked_present.marked_has_uv && has_uv) {
      result.attribute_sizes.push_back(2);
      result.attribute_types.push_back(obj::AttributeType::TexCoord);
      marked_present.marked_has_uv = true;
    }

    if (has_position) {
      for (int j = 0; j < 3; j++) {
        result.packed_data.push_back(attrib.vertices[3 * index.vertex_index + j]);
      }
    }

    if (has_normal) {
      for (int j = 0; j < 3; j++) {
        result.packed_data.push_back(attrib.normals[3 * index.normal_index + j]);
      }
    }

    if (has_uv) {
      for (int j = 0; j < 2; j++) {
        result.packed_data.push_back(attrib.texcoords[2 * index.texcoord_index + j]);
      }
    }

#ifdef GROVE_DEBUG
    if (marked_present.marked_has_position && !has_position) {
      GROVE_LOG_ERROR_CAPTURE_META("Expected positions throughout model.", "obj::load_simple");
    }
    if (marked_present.marked_has_normal && !has_normal) {
      GROVE_LOG_ERROR_CAPTURE_META("Expected normals throughout model.", "obj::load_simple");
    }
    if (marked_present.marked_has_uv && !has_uv) {
      GROVE_LOG_ERROR_CAPTURE_META("Expected uvs throughout model.", "obj::load_simple");
    }
#endif
  }
}

obj::MaterialDescriptor from_tinyobj_material(const tinyobj::material_t& material) {
  obj::MaterialDescriptor result;
  result.name = material.name;
  result.ambient_texture_name = material.ambient_texname;
  result.bump_texture_name = material.bump_texname;
  result.diffuse_texture_name = material.diffuse_texname;
  result.specular_texture_name = material.specular_texname;
  return result;
}

size_t total_num_vertices(const std::vector<tinyobj::shape_t>& shapes) {
  size_t ct{};
  for (auto& shape : shapes) {
    ct += shape.mesh.indices.size();
  }
  return ct;
}

bool faces_are_consistent(const std::vector<tinyobj::shape_t>& shapes, std::size_t* size) {
  size_t face_size{};
  for (auto& shape : shapes) {
    if (!shape.mesh.num_face_vertices.empty()) {
      face_size = shape.mesh.num_face_vertices[0];
      break;
    }
  }

  for (auto& shape : shapes) {
    for (auto& fv : shape.mesh.num_face_vertices) {
      if (fv != face_size) {
        return false;
      }
    }
  }

  *size = face_size;
  return true;
}

} //  anon

int obj::VertexData::num_vertices() const {
  auto stride = vertex_stride();
  return stride == 0 ? 0 : int(packed_data.size()) / stride;
}

Optional<int> obj::VertexData::find_attribute(AttributeType type) const {
  auto it = std::find(attribute_types.begin(), attribute_types.end(), type);
  return it == attribute_types.end() ? NullOpt{} : Optional<int>(int(it - attribute_types.begin()));
}

int obj::VertexData::vertex_stride() const {
  return std::accumulate(attribute_sizes.begin(), attribute_sizes.end(), 0);
}

obj::Data obj::load_complete(const char* file_path,
                             const char* material_directory,
                             bool* success,
                             const Params&) {
  obj::Data result;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  *success = tinyobj::LoadObj(
    &attrib, &shapes, &materials, &warn, &err, file_path, material_directory);

  maybe_log(err, warn);

  if (!*success) {
    return {};
  }

  std::size_t face_size{};
  if (!faces_are_consistent(shapes, &face_size)) {
    GROVE_LOG_ERROR_CAPTURE_META("Faces in the model must have the same number of vertices.",
                                 "obj::load_complete");
    *success = false;
    return {};

  } else if (face_size != 3) {
    GROVE_LOG_WARNING_CAPTURE_META("Non-triangulated model.", "obj::load_complete");
  }

  for (auto& mat : materials) {
    result.materials.emplace_back(from_tinyobj_material(mat));
  }

  auto total_num_verts = total_num_vertices(shapes);
  std::vector<std::size_t> material_indices(total_num_verts);
  std::vector<std::size_t> face_indices(total_num_verts);
  size_t vertex_index{};
  size_t face_index{};

  MarkedPresent marked_present{};
  for (const auto& shape : shapes) {
    assert(shape.mesh.material_ids.size() == shape.mesh.material_ids.size());
    size_t vertex_index_offset{};
    size_t face_index_in_shape{};

    for (const auto& fv : shape.mesh.num_face_vertices) {
      push_packed_data(shape, attrib, vertex_index_offset, fv, result.vertex_data, marked_present);

      for (size_t i = 0; i < face_size; i++) {
        assert(vertex_index < material_indices.size() && vertex_index < face_indices.size());
        face_indices[vertex_index] = face_index;
        material_indices[vertex_index] = shape.mesh.material_ids[face_index_in_shape];
        vertex_index++;
      }

      face_index_in_shape++;
      face_index++;
      vertex_index_offset += fv;
    }
  }

  assert(vertex_index == total_num_verts);
  result.material_indices = std::move(material_indices);
  result.face_indices = std::move(face_indices);
  return result;
}

obj::VertexData obj::load_simple(const char* file_path,
                                 const char* material_directory,
                                 bool* success) {
  obj::VertexData result;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string err;

  *success = tinyobj::LoadObj(
    &attrib, &shapes, &materials, &warn, &err, file_path, material_directory);

  maybe_log(err, warn);

  if (!*success) {
    return result;
  }

  MarkedPresent marked_present{};

  for (const auto& shape : shapes) {
    size_t index_offset = 0;

    for (const auto& fv : shape.mesh.num_face_vertices) {
#ifdef GROVE_DEBUG
      if (fv != 3) {
        GROVE_LOG_WARNING_CAPTURE_META("Non-triangulated model.", "obj::load_simple");
      }
#endif

      push_packed_data(shape, attrib, index_offset, fv, result, marked_present);
      index_offset += fv;
    }
  }

  return result;
}

GROVE_NAMESPACE_END
