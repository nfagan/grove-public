#pragma once

#include "grove/audio/audio_node.hpp"
#include "grove/audio/fdft.hpp"
#include "grove/audio/dft.hpp"
#include <cassert>

namespace grove::audio {

template <int DftSize>
bool mean_signal_amplitude(const AudioProcessBuffer& buff, const BufferChannelDescriptor& channel,
                           int num_frames, float* amp) {
  const int i0 = std::max(num_frames - DftSize, 0);
  const int i1 = std::min(i0 + DftSize, num_frames);

  if (i1 <= i0) {
    return false;
  }

  const int n = i1 - i0;
  if ((n & (n - 1)) != 0) {
    //  Expect power of two.
    return false;
  }

  assert(channel.is_float());
  float src_dft_samples[DftSize];
  float dst_dft_samples[DftSize * 2];

  for (int i = i0; i < i1; i++) {
    float v;
    channel.read(buff.data, i, &v);
    src_dft_samples[i - i0] = v;
  }

  fdft(dst_dft_samples, src_dft_samples, n);
  *amp = sum_complex_moduli(dst_dft_samples, n) / float(n);
  return true;
}

}