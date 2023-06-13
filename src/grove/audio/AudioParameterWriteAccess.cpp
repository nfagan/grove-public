#include "AudioParameterWriteAccess.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

std::atomic<uint32_t> AudioParameterWriteAccess::next_writer_id{1};

AudioParameterWriterID AudioParameterWriteAccess::create_writer() {
  AudioParameterWriterID res{next_writer_id++};
  return res;
}

bool AudioParameterWriteAccess::can_write(AudioParameterWriterID writer_id,
                                          AudioParameterIDs param_id) const {
  assert(writer_id.is_valid());
  auto it = write_access.find(param_id);
  if (it == write_access.end()) {
    return false;
  } else {
    return it->second == writer_id;
  }
}

bool AudioParameterWriteAccess::can_acquire(AudioParameterIDs param_id) const {
  return write_access.count(param_id) == 0;
}

bool AudioParameterWriteAccess::release(AudioParameterWriterID writer_id,
                                        AudioParameterIDs param_id) {
  assert(writer_id.is_valid());
  auto it = write_access.find(param_id);
  if (it != write_access.end() && it->second == writer_id) {
    write_access.erase(it);
    return true;
  } else {
    assert(false);
    return false;
  }
}

bool AudioParameterWriteAccess::request(AudioParameterWriterID writer_id,
                                        AudioParameterIDs param_id) {
  assert(writer_id.is_valid());
  auto it = write_access.find(param_id);
  if (it != write_access.end()) {
    return it->second == writer_id;
  } else {
    write_access.emplace(param_id, writer_id);
    return true;
  }
}

GROVE_NAMESPACE_END
