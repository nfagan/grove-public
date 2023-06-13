#pragma once

#include "grove/audio/audio_buffer.hpp"
#include "grove/common/Optional.hpp"
#include <vector>

namespace grove {

class AudioBuffers {
  struct Buffer {
    AudioBufferHandle handle{};
    std::string name;
  };

public:
  Optional<AudioBufferHandle> find_by_name(std::string_view name) const;

  void push(std::string name, AudioBufferHandle buffer_handle);

  bool empty() const {
    return audio_buffer_handles.empty();
  }

  static std::string audio_buffer_full_path(const char* file);
  static std::vector<std::string> default_audio_buffer_file_names();
  static const char** addtl_audio_buffer_file_names_no_max_norm(int* count);

private:
  std::vector<Buffer> audio_buffer_handles;
};

}