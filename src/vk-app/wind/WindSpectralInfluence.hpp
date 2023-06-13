#pragma once

#include "grove/audio/audio_effects/SpectrumAnalyzer.hpp"
#include "grove/common/History.hpp"

namespace grove {

class WindSpectralInfluence {
public:
  void update(const SpectrumAnalyzer::AnalysisFrame& analysis_frame);
  float current_value() const;

private:
  History<float, 10> spectral_history;
};

}