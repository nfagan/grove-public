#pragma once

#include "audio_parameters.hpp"
#include "grove/common/identifier.hpp"
#include <unordered_map>
#include <atomic>

namespace grove {

struct AudioParameterWriterID {
  GROVE_INTEGER_IDENTIFIER_EQUALITY(AudioParameterWriterID, id)
  GROVE_INTEGER_IDENTIFIER_INEQUALITIES(AudioParameterWriterID, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)

  uint32_t id{};
};

class AudioParameterWriteAccess {
public:
  struct ScopedAccess {
    ScopedAccess(AudioParameterWriteAccess& access, AudioParameterWriterID writer,
                 AudioParameterIDs param) :
      access{access}, writer{writer}, param{param} {
      acquired = access.request(writer, param);
    }
    ~ScopedAccess() {
      if (acquired) {
        access.release(writer, param);
      }
    }

    AudioParameterWriteAccess& access;
    AudioParameterWriterID writer;
    AudioParameterIDs param;
    bool acquired;
  };

public:
  static AudioParameterWriterID create_writer();

  bool request(AudioParameterWriterID writer_id, AudioParameterIDs param_id);
  bool request(AudioParameterWriterID writer_id, const AudioParameterDescriptor& descriptor) {
    return request(writer_id, descriptor.ids);
  }

  bool release(AudioParameterWriterID writer_id, AudioParameterIDs param_id);
  bool release(AudioParameterWriterID writer_id, const AudioParameterDescriptor& descriptor) {
    return release(writer_id, descriptor.ids);
  }

  bool can_write(AudioParameterWriterID writer_id, AudioParameterIDs param_id) const;

  bool can_acquire(AudioParameterIDs param_id) const;
  bool can_acquire(const AudioParameterDescriptor& descriptor) const {
    return can_acquire(descriptor.ids);
  }

  int num_in_use() const {
    return int(write_access.size());
  }

private:
  std::unordered_map<AudioParameterIDs, AudioParameterWriterID, AudioParameterIDs::Hash> write_access;

private:
  static std::atomic<uint32_t> next_writer_id;
};

}