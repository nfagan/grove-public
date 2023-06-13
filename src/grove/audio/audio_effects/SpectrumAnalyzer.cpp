#include "SpectrumAnalyzer.hpp"
#include "../dft.hpp"
#include "grove/common/logging.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

AudioEvent make_dft_event(uint32_t id) {
  auto evt = make_audio_event(AudioEvent::Type::NewDFTFrame, {});
  evt.id = id;
  return evt;
}

} //  anon

/*
* TODO: Fix event handling; handle change to enable / disable
*/

SpectrumAnalyzer::SpectrumAnalyzer() :
  samples(new Sample[block_size]{}),
  frame_index(0),
  interval_index(0),
  enabled(true) {
  //
  initialize_free_spectra();
}

void SpectrumAnalyzer::initialize_free_spectra() {
  while (free_spectra.num_free() > 0) {
    free_spectra.write({std::make_unique<Sample[]>(block_size * 2)});
  }
}

void SpectrumAnalyzer::process(Sample* source_samples,
                               AudioEvents* out_events,
                               const AudioParameterChangeView&,
                               const AudioRenderInfo& info) {
  const auto refresh_frames = int(refresh_interval * info.sample_rate);

  for (int i = 0; i < info.num_frames; i++) {
    samples[frame_index] = source_samples[i * info.num_channels];

    if (interval_index >= refresh_frames) {
      if (free_spectra.size() > 0 && !pending_spectra.full()) {
        auto frame = free_spectra.read();
        frame.id = next_analysis_frame_id++;
        dft(source_samples, frame.buffer.get(), block_size);

        out_events[i].push_back(make_dft_event(frame.id));
        pending_spectra.write(std::move(frame));

      } else {
#if 0
        GROVE_LOG_WARNING_CAPTURE_META("Could not generate spectrum; no free spectra.",
                                       "SpectrumAnalyzer");
#endif
      }

      interval_index = 0;
    } else {
      interval_index++;
    }

    frame_index = (frame_index + 1) % block_size;
  }
}

void SpectrumAnalyzer::read_pending_spectra(SpectrumAnalyzer::ReadFrames& frames) noexcept {
  int num_read = pending_spectra.size();

  for (int i = 0; i < num_read; i++) {
    auto frame = pending_spectra.read();
    assert(frames.count(frame.id) == 0);
    frames[frame.id] = std::move(frame);
  }
}

void SpectrumAnalyzer::return_pending_spectrum(AnalysisFrame frame) {
  if (!free_spectra.maybe_write(std::move(frame))) {
    GROVE_LOG_WARNING_CAPTURE_META("Could not return free spectrum; buffer full.",
                                   "SpectrumAnalyzer");
  }
}

void SpectrumAnalyzer::enable() {
  enabled.store(true);
}

void SpectrumAnalyzer::disable() {
  enabled.store(false);
}

bool SpectrumAnalyzer::is_enabled() const {
  return enabled.load();
}

GROVE_NAMESPACE_END
