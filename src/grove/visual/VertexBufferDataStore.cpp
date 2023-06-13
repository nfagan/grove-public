#include "VertexBufferDataStore.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

VertexBufferDataStore::Handle VertexBufferDataStore::emplace(Meta meta,
                                                             std::unique_ptr<unsigned char[]> data,
                                                             std::size_t size,
                                                             VertexBufferDescriptor descriptor) {
  Entry entry{};
  entry.data.data = data.get();
  entry.data.size = size;
  entry.data.descriptor = std::move(descriptor);
  entry.meta = std::move(meta);

  InMemoryBackingStoreEntry backing_store_entry{};
  backing_store_entry.data = std::move(data);

  Handle handle{next_handle_id++};
  entries[handle] = std::move(entry);
  in_memory_backing_store[handle] = std::move(backing_store_entry);

  return handle;
}

const VertexBufferDataStore::Entry* VertexBufferDataStore::lookup(Handle handle) const {
  if (auto it = entries.find(handle); it != entries.end()) {
    return &it->second;
  } else {
    return nullptr;
  }
}

const VertexBufferDataStore::Entry*
VertexBufferDataStore::search_by_file(const std::string& file, Handle* out_handle) const {
  for (auto& entry_it : entries) {
    if (entry_it.second.meta.file == file) {
      *out_handle = entry_it.first;
      return &entry_it.second;
    }
  }
  return nullptr;
}

void VertexBufferDataStore::erase(Handle handle) {
  entries.erase(handle);
  in_memory_backing_store.erase(handle);
}

GROVE_NAMESPACE_END
