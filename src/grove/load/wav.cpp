#include "wav.hpp"
#include "grove/math/util.hpp"

#include <fstream>
#include <iostream>
#include <cassert>
#include <limits>

namespace grove {

namespace {

//constexpr int chunk_descriptor_bytes = 12;
//constexpr int fmt_descriptor_bytes = 24;
constexpr int fmt_channel_offset = 22;
constexpr int data_offset_bytes = 44;

bool read_format_descriptor(std::ifstream& file, wav::FormatDescriptor* descriptor) {
  file.seekg(fmt_channel_offset);
  file.read((char*) &descriptor->num_channels, sizeof(uint16_t));
  file.read((char*) &descriptor->sample_rate, sizeof(uint32_t));
  file.read((char*) &descriptor->byte_rate, sizeof(uint32_t));
  file.read((char*) &descriptor->block_align, sizeof(uint16_t));
  file.read((char*) &descriptor->bits_per_sample, sizeof(uint16_t));

  switch (descriptor->bits_per_sample) {
    case 8:
      descriptor->source_format = wav::SourceFormat::UInt8;
      break;
    case 16:
      descriptor->source_format = wav::SourceFormat::Int16;
      break;
    default:
      //  Unrecognized format.
      return false;
  }

  return true;
}

bool read_data_sub_chunk_size(std::ifstream& file, uint32_t* chunk_size) {
  char data_id[5] = {0};
  std::fill(data_id, data_id + 5, char(0));
  file.read(data_id, 4);

  if (std::strcmp(data_id, "data") != 0) {
    return false;
  }

  file.read((char*) chunk_size, sizeof(uint32_t));
  return true;
}

}

/*
 * FileReadResult
 */

bool wav::FileReadResult::success() const {
  return error == Error::NoError;
}

/*
 * read_wav_file
 */

wav::FileReadResult wav::read_wav_file(const std::string& file_path) {
  using Err = FileReadResult::Error;
  wav::FileReadResult result{};

  std::ifstream wav_file;
  wav_file.open(file_path.c_str(), std::ios_base::in | std::ios_base::binary);

  if (!wav_file.good()) {
    return result;
  }

  wav_file.seekg(0, wav_file.end);
  const auto length = wav_file.tellg();
  wav_file.seekg(0, wav_file.beg);

  if (length < data_offset_bytes) {
    result.error = Err::InvalidFormat;
    return result;
  }

  const bool fmt_success = read_format_descriptor(wav_file, &result.format_descriptor);
  if (!fmt_success) {
    result.error = Err::InvalidFormat;
    return result;
  }

  uint32_t chunk_size;
  const bool chunk_success = read_data_sub_chunk_size(wav_file, &chunk_size);
  if (!chunk_success) {
    result.error = Err::InvalidFormat;
    return result;
  }

  if (int(length) - data_offset_bytes != int(chunk_size)) {
    result.error = Err::InvalidFormat;
    return result;
  }

  const auto bytes_per_sample = result.format_descriptor.bits_per_sample / 8;
  const auto num_samples = chunk_size / bytes_per_sample;
  const auto num_channels = result.format_descriptor.num_channels;

  result.format_descriptor.num_samples = num_samples;
  result.format_descriptor.num_frames = num_samples / num_channels;

  result.data.reset(new unsigned char[chunk_size]);
  wav_file.read((char*) result.data.get(), chunk_size);
  result.error = Err::NoError;

  return result;
}

/*
 * wav_file_data_to_float
 */

std::unique_ptr<float[]>
wav::wav_file_data_to_float(const FileReadResult& res, bool normalize, bool max_normalize) {
  if (res.error != FileReadResult::Error::NoError) {
    return nullptr;
  }

  auto out = std::make_unique<float[]>(res.format_descriptor.num_samples);

  if (res.format_descriptor.source_format == wav::SourceFormat::UInt8) {
    constexpr auto uint8_max = float(std::numeric_limits<uint8_t>::max());
    constexpr float uint8_min = 0.0f;

    for (uint32_t i = 0; i < res.format_descriptor.num_samples; i++) {
      auto val = float(uint8_t(res.data[i]));

      if (normalize) {
        val = (val - uint8_min) / (uint8_max - uint8_min) * 2.0f - 1.0f;
      }

      out[i] = val;
    }

  } else if (res.format_descriptor.source_format == wav::SourceFormat::Int16) {
    constexpr auto int16_max = float(std::numeric_limits<int16_t>::max());
    constexpr auto int16_min = float(std::numeric_limits<int16_t>::min());

    const auto* src = res.data.get();
    uint32_t off = 0;

    for (uint32_t i = 0; i < res.format_descriptor.num_samples; i++) {
      int16_t v;
      std::memcpy(&v, src + off, 2);
      off += 2;
      auto val = float(v);

      if (normalize) {
        val = (val - int16_min) / (int16_max - int16_min) * 2.0f - 1.0f;
      }

      out[i] = val;
    }

  } else {
    assert(false && "Unhandled source format.");
  }

  if (max_normalize) {
    abs_max_normalize(out.get(), out.get() + res.format_descriptor.num_samples);
  }

  return out;
}

}
