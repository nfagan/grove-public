#include "serialize_generic.hpp"
#include "grove/common/common.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace tree::io;

struct ReadStream {
  const unsigned char* data;
  size_t size;
};

template <typename T>
bool read(const ReadStream& stream, size_t& off, T* out) {
  if (off + sizeof(T) > stream.size) {
    return false;
  } else {
    memcpy(out, stream.data + off, sizeof(T));
    off += sizeof(T);
    return true;
  }
}

template <typename T>
bool deserialize_vector(const ReadStream& stream, size_t& off, std::vector<T>* out) {
  size_t size;
  if (!read(stream, off, &size)) {
    return false;
  }

  assert(size < (~0u));
  *out = std::vector<T>(size);
  for (size_t i = 0; i < size; i++) {
    if (!read(stream, off, &(*out)[i])) {
      return false;
    }
  }
  return true;
}

} //  anon

Optional<std::vector<Node>> tree::io::deserialize(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    return NullOpt{};
  }

  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<unsigned char> buffer(size);
  file.read((char*) buffer.data(), size);

  ReadStream stream{buffer.data(), size_t(size)};
  std::vector<Node> result;

  size_t off{};
  if (deserialize_vector(stream, off, &result)) {
    return Optional<std::vector<Node>>(std::move(result));
  } else {
    return NullOpt{};
  }
}

bool tree::io::serialize(const Node* nodes, int num_nodes, const std::string& file_path) {
  std::ofstream file(file_path, std::ios::binary);
  if (!file.good()) {
    return false;
  } else {
    const auto num_nodes64 = size_t(num_nodes);
    file.write((char*) &num_nodes64, sizeof(size_t));
    file.write((char*) nodes, int64_t(num_nodes * sizeof(Node)));
    return true;
  }
}

GROVE_NAMESPACE_END
