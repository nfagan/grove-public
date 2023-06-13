#pragma once

struct PaStreamCallbackTimeInfo;

namespace grove::audio {
  int callback(const void* input_buffer, void* output_buffer,
               unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info,
               unsigned long status, void* renderer) noexcept;
}