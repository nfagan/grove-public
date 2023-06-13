#pragma once

#include "types.hpp"
#include "grove/common/identifier.hpp"
#include <string>
#include <unordered_map>
#include <memory>

namespace grove {

class VertexBufferDataStore {
public:
  enum class Origin {
    File
  };

  struct Handle {
    GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, Handle, id)
    GROVE_INTEGER_IDENTIFIER_EQUALITY(Handle, id)
    GROVE_INTEGER_IDENTIFIER_INEQUALITIES(Handle, id)

    uint32_t id{};
  };

  struct Meta {
    Origin origin{Origin::File};
    std::string file;
    std::string material_directory;
  };

  struct Data {
    const unsigned char* data{};
    std::size_t size{};
    VertexBufferDescriptor descriptor;
  };

  struct Entry {
    Meta meta{};
    Data data{};
  };

  struct InMemoryBackingStoreEntry {
    std::unique_ptr<unsigned char[]> data{};
  };

public:
  Handle emplace(Meta meta,
                 std::unique_ptr<unsigned char[]> data,
                 std::size_t size,
                 VertexBufferDescriptor descriptor);
  void erase(Handle handle);
  const Entry* lookup(Handle handle) const;
  const Entry* search_by_file(const std::string& file, Handle* handle) const;

private:
  std::unordered_map<Handle, Entry, Handle::Hash> entries;
  std::unordered_map<Handle, InMemoryBackingStoreEntry, Handle::Hash> in_memory_backing_store;

  uint32_t next_handle_id{1};
};

}