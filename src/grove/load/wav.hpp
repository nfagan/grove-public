#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace grove::wav {
  enum class SourceFormat {
    UInt8,
    Int16
  };

  struct FormatDescriptor {
    uint16_t num_channels{};
    uint32_t sample_rate{};
    uint32_t byte_rate{};
    uint16_t block_align{};
    uint16_t bits_per_sample{};
    uint32_t num_samples{};
    uint32_t num_frames{};
    SourceFormat source_format{SourceFormat::UInt8};
  };

  struct FileReadResult {
    enum class Error {
      NoError = 0,
      ReadingFile,
      InvalidFormat
    };

    bool success() const;

    Error error{Error::ReadingFile};
    FormatDescriptor format_descriptor;
    std::unique_ptr<unsigned char[]> data;
  };

  FileReadResult read_wav_file(const std::string& file_path);

  std::unique_ptr<float[]> wav_file_data_to_float(const FileReadResult& result,
                                                  bool normalize = true,
                                                  bool max_normalize = false);
}