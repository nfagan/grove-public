#pragma once

#include "audio_buffer.hpp"

namespace grove::io {

struct ReadAudioBufferResult {
  bool success{false};
  AudioBufferDescriptor descriptor;
  std::unique_ptr<unsigned char[]> data;
};

bool write_audio_buffer(const AudioBufferDescriptor& descriptor,
                        const void* data, const char* to_file);

ReadAudioBufferResult read_wav_as_float(const char* file,
                                        bool normalize = true,
                                        bool max_normalize = false);

#if 0
ReadAudioBufferResult read_audio_buffer(const char* from_file);
#endif

}