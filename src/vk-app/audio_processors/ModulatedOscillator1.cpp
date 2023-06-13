#include "ModulatedOscillator1.hpp"
#include "pitch_cv.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr double amplitude_mod_depth() {
  return 1.0;
}

constexpr double frequency_mod_depth() {
  return 5.0;
}

using PitchCVMap = audio::PitchCVMap;

inline float cv_to_frequency(float cv, const Tuning& tuning) {
  auto norm = clamp(double(cv), PitchCVMap::min_cv, PitchCVMap::max_cv);
  norm = (norm - PitchCVMap::min_cv) / PitchCVMap::cv_span;
  const double st = norm * PitchCVMap::semitone_span + PitchCVMap::min_semitone;
  return float(semitone_to_frequency_equal_temperament(st, tuning));
}

} //  anon

ModulatedOscillator1::ModulatedOscillator1(const AudioScale* scale) :
  scale{scale},
  oscillator{default_sample_rate(), frequency_a4()},
  center_frequency{frequency_a4()} {
  //
  auto opt_flags = AudioPort::Flags::marked_optional();
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 0, opt_flags}); //  frequency
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 1, opt_flags}); //  amplitude
  input_ports.push_back(InputAudioPort{BufferDataType::Float, this, 2, opt_flags}); //  freq-mod

  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 0});
  output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, 1});

  oscillator.fill_tri(4);
  oscillator.normalize();
}

void ModulatedOscillator1::process(const AudioProcessData& in,
                                   const AudioProcessData& out,
                                   AudioEvents*,
                                   const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  const auto& frequency = in.descriptors[0];
  const auto& amp_mod = in.descriptors[1];
  const auto& freq_mod = in.descriptors[2];

  oscillator.set_sample_rate(info.sample_rate);
  const auto* tuning = scale->render_get_tuning();
  center_frequency = cv_to_frequency(current_cv, *tuning);

  for (int i = 0; i < info.num_frames; i++) {
    float v{};

    if (!frequency.is_missing()) {
      frequency.read(in.buffer.data, i, &current_cv);
      center_frequency = cv_to_frequency(current_cv, *tuning);
    }
    if (!amp_mod.is_missing()) {
      amp_mod.read(in.buffer.data, i, &osc_params.amplitude);
    }
    if (!freq_mod.is_missing()) {
      freq_mod.read(in.buffer.data, i, &osc_params.frequency_modulation);
    }

    auto freq = center_frequency + osc_params.frequency_modulation * frequency_mod_depth();
    oscillator.set_frequency(freq);
    v += float(oscillator.tick() * (osc_params.amplitude * amplitude_mod_depth()));

    for (auto& descriptor : out.descriptors) {
      descriptor.write(out.buffer.data, i, &v);
    }
  }
}

OutputAudioPorts ModulatedOscillator1::outputs() const {
  return output_ports;
}

InputAudioPorts ModulatedOscillator1::inputs() const {
  return input_ports;
}

GROVE_NAMESPACE_END

