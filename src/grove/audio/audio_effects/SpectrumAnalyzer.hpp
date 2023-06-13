#pragma once

#include "../AudioEffect.hpp"
#include "grove/common/RingBuffer.hpp"
#include <atomic>
#include <unordered_map>

namespace grove {

class SpectrumAnalyzer : public AudioEffect {
public:
  static constexpr int block_size = 128;
  static constexpr double refresh_interval = 10e-3;

public:
  using SampleBuffer = std::unique_ptr<Sample[]>;

  struct AnalysisFrame {
    static constexpr int block_size = SpectrumAnalyzer::block_size;
    static constexpr int size = block_size * 2;

    SampleBuffer buffer;
    uint32_t id{};
  };

  using ReadFrames = std::unordered_map<uint32_t, AnalysisFrame>;

public:
  SpectrumAnalyzer();
  ~SpectrumAnalyzer() override = default;

  void process(Sample* source_samples,
               AudioEvents* out_events,
               const AudioParameterChangeView& parameter_changes,
               const AudioRenderInfo& info) override;

  void enable() override;
  void disable() override;
  bool is_enabled() const override;

  void read_pending_spectra(ReadFrames& frames) noexcept;
  void return_pending_spectrum(AnalysisFrame frame);

private:
  void initialize_free_spectra();

private:
  SampleBuffer samples;
  int frame_index;
  int interval_index;

  RingBuffer<AnalysisFrame, 20> free_spectra;
  RingBuffer<AnalysisFrame, 20> pending_spectra;

  std::atomic<bool> enabled;
  uint32_t next_analysis_frame_id{1};
};

}