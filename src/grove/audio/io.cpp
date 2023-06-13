#include "io.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include "grove/load/wav.hpp"
#include <fstream>

GROVE_NAMESPACE_BEGIN

namespace {

#if GROVE_LOGGING_ENABLED
constexpr const char* logging_id() {
  return "audio/io";
}
#endif

} //  anon

bool io::write_audio_buffer(const AudioBufferDescriptor& descriptor,
                            const void* data,
                            const char* file_path) {
  std::fstream file;
  file.open(file_path, std::ios_base::out | std::ios_base::binary);

  if (!file.good()) {
    std::string msg{"Failed to open file: "};
    msg += file_path;
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
    return false;
  }

  auto num_channels = int(descriptor.layout.num_channels());
  uint32_t stride = descriptor.layout.stride();
  auto num_frames = int64_t(descriptor.size / stride);

  if (std::size_t(num_frames) * std::size_t(stride) != descriptor.size) {
    GROVE_LOG_ERROR_CAPTURE_META("Num frames does not match stride.", logging_id());
    return false;
  }

  double sample_rate = descriptor.sample_rate;

  file.write((char*) &sample_rate, sizeof(sample_rate));
  file.write((char*) &num_channels, sizeof(num_channels));

  for (int i = 0; i < num_channels; i++) {
    auto descr = descriptor.layout.channel_descriptor(i);
    auto type = int(descr.type);
    int offset = descr.offset;

    file.write((char*) &type, sizeof(type));
    file.write((char*) &offset, sizeof(offset));
  }

  file.write((char*) &num_frames, sizeof(num_frames));
  file.write((char*) &stride, sizeof(stride));
  file.write((char*) data, descriptor.size);

  return true;
}

io::ReadAudioBufferResult io::read_wav_as_float(const char* file,
                                                bool normalize,
                                                bool max_normalize) {
  ReadAudioBufferResult result{};

  auto res = wav::read_wav_file(file);
  if (res.success()) {
    auto data = wav::wav_file_data_to_float(res, normalize, max_normalize);

    if (data) {
      auto& fmt = res.format_descriptor;
      result.descriptor = AudioBufferDescriptor::from_interleaved_float(
        fmt.sample_rate, fmt.num_frames, fmt.num_channels);

      auto recoded_data = std::make_unique<unsigned char[]>(result.descriptor.size);
      if (result.descriptor.size > 0) {
        std::memcpy(recoded_data.get(), data.get(), result.descriptor.size);
      }

      result.data = std::move(recoded_data);
      result.success = true;
    } else {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to convert wav data to float.", logging_id());
    }

  } else {
    std::string message{"Failed to read: "};
    message += file;
    GROVE_LOG_ERROR_CAPTURE_META(message.c_str(), logging_id());
  }

  return result;
}

#if 0
io::ReadAudioBufferResult io::read_audio_buffer(const char* file_path) {
  ReadAudioBufferResult result;

  std::fstream file;
  file.open(file_path, std::ios_base::in | std::ios_base::binary);

  if (!file.good()) {
    std::string msg{"Failed to open file: "};
    msg += file_path;
    GROVE_LOG_ERROR_CAPTURE_META(msg.c_str(), logging_id());
    return result;
  }
}
#endif

GROVE_NAMESPACE_END
