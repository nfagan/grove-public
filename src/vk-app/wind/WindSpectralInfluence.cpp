#include "WindSpectralInfluence.hpp"
#include "grove/common/common.hpp"
#include "grove/common/stats.hpp"
#include <cmath>

GROVE_NAMESPACE_BEGIN

void WindSpectralInfluence::update(const SpectrumAnalyzer::AnalysisFrame& frame) {
  double mag = 0.0;

  for (int i = 0; i < SpectrumAnalyzer::AnalysisFrame::block_size; i++) {
    const float re = frame.buffer[i*2];
    const float im = frame.buffer[i*2 + 1];
    mag += std::sqrt(re * re + im * im);
  }

  mag /= double(SpectrumAnalyzer::AnalysisFrame::block_size);
  spectral_history.push(float(mag * 1e2));
}

float WindSpectralInfluence::current_value() const {
  auto curr = std::max(0.0f, spectral_history.mean_or_default(0.0f));
  auto v = 1.0f - std::exp(-curr);
  return v;
}

GROVE_NAMESPACE_END
