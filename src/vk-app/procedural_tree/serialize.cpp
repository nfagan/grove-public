#include "serialize.hpp"
#include "grove/common/common.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

using namespace tree;

namespace {

using WriteStream = std::vector<unsigned char>;
using File = std::fstream;

struct ReadStream {
  const unsigned char* data;
  size_t size;
};

template <typename T>
void write(WriteStream& stream, const T* data) {
  size_t size = stream.size();
  stream.resize(stream.size() + sizeof(T));
  memcpy(stream.data() + size, data, sizeof(T));
}

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
void serialize_vector(WriteStream& out, const std::vector<T>& vec) {
  size_t size = vec.size();
  write(out, &size);
  for (auto& el : vec) {
    write<T>(out, &el);
  }
}

template <typename T>
bool deserialize_vector(const ReadStream& stream, size_t& off, std::vector<T>* out) {
  size_t size;
  if (!read(stream, off, &size)) {
    return false;
  }
  *out = std::vector<T>(size);
  for (size_t i = 0; i < size; i++) {
    if (!read(stream, off, &(*out)[i])) {
      return false;
    }
  }
  return true;
}

void serialize(WriteStream& out, const std::vector<Internode>& inodes) {
  serialize_vector(out, inodes);
}

bool deserialize(const ReadStream& stream, size_t& off, std::vector<Internode>* out) {
  if (deserialize_vector(stream, off, out)) {
    for (auto& node : *out) {
      node.id = TreeInternodeID::create();
    }
    return true;
  } else {
    return false;
  }
}

void serialize(WriteStream& out, const std::vector<Bud>& buds) {
  serialize_vector(out, buds);
}

bool deserialize(const ReadStream& stream, size_t& off, std::vector<Bud>* out) {
  if (deserialize_vector(stream, off, out)) {
    for (auto& bud : *out) {
      bud.id = TreeBudID::create();
    }
    return true;
  } else {
    return false;
  }
}

} //  anon

std::vector<unsigned char> tree::serialize(const TreeNodeStore& store) {
  std::vector<unsigned char> result;
  grove::serialize(result, store.internodes);
  grove::serialize(result, store.buds);
  return result;
}

Optional<TreeNodeStore> tree::deserialize(const unsigned char* data, size_t size) {
  ReadStream read{data, size};
  TreeNodeStore result;
  size_t off{};
  if (!deserialize(read, off, &result.internodes)) {
    return NullOpt{};
  }
  if (!deserialize(read, off, &result.buds)) {
    return NullOpt{};
  }
  result.id = TreeID::create();
  return Optional<TreeNodeStore>(std::move(result));
}

bool tree::serialize_file(const TreeNodeStore& store, const char* file_path) {
  File file;
  file.open(file_path, std::ios_base::out | std::ios_base::binary);
  if (!file.good()) {
    return false;
  }
  auto data = serialize(store);
  file.write((char*) data.data(), (std::streamsize) data.size());
  return file.good();
}

Optional<TreeNodeStore> tree::deserialize_file(const char* file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    return NullOpt{};
  }

  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<unsigned char> buffer(size);
  if (file.read((char*) buffer.data(), size)) {
    return deserialize(buffer.data(), buffer.size());
  } else {
    return NullOpt{};
  }
}

GROVE_NAMESPACE_END
