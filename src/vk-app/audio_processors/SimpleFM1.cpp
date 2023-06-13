#include "SimpleFM1.hpp"
#include "signal.hpp"
#include "grove/audio/AudioEventSystem.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/common/common.hpp"
#include "grove/math/ease.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

constexpr int num_params = 4;

} //  anon

SimpleFM1::SimpleFM1(uint32_t node_id, const AudioParameterSystem* param_sys, const AudioScale* scale) :
  node_id{node_id},
  param_sys{param_sys},
  scale{scale} {
  //
  carrier_frequency.set_time_constant95(5e-3f);
}

InputAudioPorts SimpleFM1::inputs() const {
  auto opt_flag = AudioPort::Flags::marked_optional();
  InputAudioPorts result;
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<SimpleFM1*>(this), 0});
  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<SimpleFM1*>(this), 1, opt_flag});
  result.push_back(InputAudioPort{BufferDataType::Float, const_cast<SimpleFM1*>(this), 2, opt_flag});
  return result;
}

OutputAudioPorts SimpleFM1::outputs() const {
  OutputAudioPorts result;
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<SimpleFM1*>(this), 0});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<SimpleFM1*>(this), 1});
  return result;
}

void SimpleFM1::process(const AudioProcessData& in, const AudioProcessData& out,
                        AudioEvents*, const AudioRenderInfo& info) {
  const auto& changes = param_system::render_read_changes(param_sys);
  auto self_changes = changes.view_by_parent(node_id);

  decltype(&fd_freq) params[num_params]{&fd_freq, &fm_freq, &fm_depth, &detune};
  for (int i = 0; i < num_params; i++) {
    auto p_change = self_changes.view_by_parameter(i);
    AudioParameterChange change{};
    if (p_change.collapse_to_last_change(&change)) {
      params[i]->apply(change);
    }
  }

  const double period = two_pi() / info.sample_rate;
  const auto* tuning = scale->render_get_tuning();
  carrier_frequency.set_target(
    float(note_number_to_frequency_equal_temperament(note_num, *tuning)));

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      note_num = message.note_number();
      carrier_frequency.set_target(
        float(note_number_to_frequency_equal_temperament(note_num, *tuning)));
    }

    float afm{};
    if (!in.descriptors[1].is_missing()) {
      in.descriptors[1].read(in.buffer.data, i, &afm);
      afm = clamp(afm, -1.0f, 1.0f);
    }

    float g{1.0f};
    if (!in.descriptors[2].is_missing()) {
      in.descriptors[2].read(in.buffer.data, i, &g);
      g = clamp01(std::abs(g));
    }

    float fc = carrier_frequency.tick(float(info.sample_rate));
    fc *= std::pow(2.0f, 0.2f * (detune.evaluate() * 2.0f - 1.0f) / 12.0f);

    const float fmm_v = lerp(ease::log(fm_freq.evaluate(), 10.0f), 1.0f, 64.0f);
    const float fd = lerp(fd_freq.evaluate(), 0.5f, 64.0f);
    float depth = ease::log(fm_depth.evaluate(), 10.0f);
    depth += depth * afm * 0.25f;

    auto gfm = float(std::sin(modulator_phase));
    modulator_phase += period * fmm_v;
    osc::detail::iterative_wrap_phase(&modulator_phase, two_pi());

    const double phase_mod_arg = depth * 0.5 * fc * fd / fmm_v * gfm;

    auto s = float(std::cos(carrier_phase + phase_mod_arg));
    carrier_phase += period * fc;
    osc::detail::iterative_wrap_phase(&carrier_phase, two_pi());

    s *= g;

    for (int j = 0; j < 2; j++) {
      out.descriptors[j].write(out.buffer.data, i, &s);
    }
  }

  if (info.num_frames > 0) {
    float v{};
    if (audio::mean_signal_amplitude<64>(out.buffer, out.descriptors[0], info.num_frames, &v)) {
      const float min_db = -50.0f;
      const float max_db = 12.0f;
      v = (clamp(float(amplitude_to_db(v)), min_db, max_db) - min_db) / (max_db - min_db);

      auto stream = audio_event_system::default_event_stream();
      const int write_frame = info.num_frames - 1;
      auto evt = make_monitorable_parameter_audio_event(
        {node_id, num_params}, make_float_parameter_value(v), write_frame, 0);
      (void) audio_event_system::render_push_event(stream, evt);
    }
  }
}

void SimpleFM1::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto* dst = mem.push(num_params + 1);
  int i{};
  uint32_t p{};

  AudioParameter<float, StaticLimits01<float>> repr_p;
  auto flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  dst[i++] = fd_freq.make_descriptor(node_id, p++, 0.0f, "frequency_deviation");
  dst[i++] = fm_freq.make_descriptor(node_id, p++, 0.0f, "modulator_frequency");
  dst[i++] = fm_depth.make_descriptor(node_id, p++, 0.0f, "fm_depth");
  dst[i++] = detune.make_descriptor(node_id, p++, 0.5f, "detune");
  dst[i++] = repr_p.make_descriptor(node_id, p++, 0.0f, "signal_representation", flags);
  assert(i == num_params + 1);
}

GROVE_NAMESPACE_END
