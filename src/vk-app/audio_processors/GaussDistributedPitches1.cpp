#include "GaussDistributedPitches1.hpp"
#include "gauss_distributed_pitches.hpp"
#include "gauss_distributed_pitches_impl.hpp"
#include "parameter.hpp"
#include "grove/audio/AudioScale.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

struct {
  gdp::Distribution distribution;
  bool initialized_distribution{};
} globals;

} //  anon

GaussDistributedPitches1::GaussDistributedPitches1(
  uint32_t node_id, const AudioScale* scale, const AudioParameterSystem* param_sys) :
  node_id{node_id}, scale{scale}, param_sys{param_sys} {
  //
  kb_semitone = float(midi_note_number_c3());

  if (!globals.initialized_distribution) {
    initialize(&globals.distribution);
    update_distribution();
    globals.initialized_distribution = true;
  }
}

InputAudioPorts GaussDistributedPitches1::inputs() const {
  InputAudioPorts result;
  result.push_back(InputAudioPort{
    BufferDataType::MIDIMessage, const_cast<GaussDistributedPitches1*>(this), 0});
  return result;
}

OutputAudioPorts GaussDistributedPitches1::outputs() const {
  OutputAudioPorts result;
  for (int i = 0; i < 2; i++) {
    result.push_back(OutputAudioPort{
      BufferDataType::Float, const_cast<GaussDistributedPitches1*>(this), i});
  }
  return result;
}

void GaussDistributedPitches1::update_distribution() {
  float mus[num_lobes];
  float sigmas[num_lobes];
  float scales[num_lobes];
  for (int i = 0; i < num_lobes; i++) {
    mus[i] = float(params.mus[i].value) + float(gdp::Config::ref_st);
    sigmas[i] = lerp(clamp01(params.sigmas[i].value), min_sigma, max_sigma);
    scales[i] = clamp01(params.scales[i].value);
  }
  update(&globals.distribution, mus, sigmas, scales, num_lobes);
}

void GaussDistributedPitches1::process(
  const AudioProcessData& in, const AudioProcessData& out,
  AudioEvents*, const AudioRenderInfo& info) {
  //
#if GROVE_PREFER_AUDIO_SCALE_SYS
  const auto* scale_sys = scale_system::get_global_audio_scale_system();
  (void) scale;
#else
  const auto& tuning = *scale->render_get_tuning();
#endif

  {
    const auto& changes = param_system::render_read_changes(param_sys);
    auto self_changes = changes.view_by_parent(node_id);

    bool need_modify{};
    uint32_t pi{};
    for (auto& p : params.mus) {
      if (auto v = check_apply_int_param(p, self_changes.view_by_parameter(pi++))) {
        need_modify = true;
      }
    }
    for (auto& p : params.sigmas) {
      if (check_immediate_apply_float_param(p, self_changes.view_by_parameter(pi++))) {
        need_modify = true;
      }
    }
    for (auto& p : params.scales) {
      if (check_immediate_apply_float_param(p, self_changes.view_by_parameter(pi++))) {
        need_modify = true;
      }
    }

    (void) check_apply_int_param(params.follow_keyboard, self_changes.view_by_parameter(pi++));

    if (need_modify) {
      update_distribution();
    }
  }

  const bool pitch_follow_kb = params.follow_keyboard.value == 1;
  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      kb_semitone = message.note_number();
    }

    float accum{};
    for (auto& v : voices) {
      if (v.on) {
        v.duration = std::max(0.0, v.duration - 1.0 / info.sample_rate);
        if (v.duration == 0.0) {
          v.on = false;
        }
      }
      if (!v.on) {
        float st = sample(&globals.distribution, urand()) - float(gdp::Config::ref_st);
        if (pitch_follow_kb) {
          v.st_has_kb_offset = false;
        } else {
          st += kb_semitone;
          v.st_has_kb_offset = true;
        }
        v.duration = lerp(urand(), 100e-3, 1000e-3);
        v.on = true;
        v.st = st;
      }

      const float st = v.st_has_kb_offset ? v.st : v.st + kb_semitone;
#if GROVE_PREFER_AUDIO_SCALE_SYS
      double f = scale_system::render_get_frequency_from_semitone(scale_sys, st, i);
#else
      double f = semitone_to_frequency_equal_temperament(st, tuning);
#endif
      double s = osc::Sin::tick(info.sample_rate, &v.phase, f);
      accum += float(s);
    }

    accum /= float(num_voices);
    out.descriptors[0].write(out.buffer.data, i, &accum);
    out.descriptors[1].write(out.buffer.data, i, &accum);
  }
}

void GaussDistributedPitches1::parameter_descriptors(
  TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  //
  const char* mu_names[8]{"mu0", "mu1", "mu2", "mu3", "mu4", "mu5", "mu6", "mu7"};
  const char* sigma_names[8]{"sigma0", "sigma1", "sigma2", "sigma3", "sigma4", "sigma5", "sigma6", "sigma7"};
  const char* scale_names[8]{"scale0", "scale1", "scale2", "scale3", "scale4", "scale5", "scale6", "scale7"};
  static_assert(num_lobes <= 8);

  auto* dst = mem.push(Params::num_params);
  Params p;
  int i{};
  uint32_t pi{};
  int ind;

  ind = 0;
  for (auto& mu : p.mus) {
    dst[i++] = mu.make_descriptor(node_id, pi++, mu.value, mu_names[ind++]);
  }

  ind = 0;
  for (auto& sigma : p.sigmas) {
    dst[i++] = sigma.make_descriptor(node_id, pi++, sigma.value, sigma_names[ind++]);
  }

  ind = 0;
  for (auto& scl : p.scales) {
    dst[i++] = scl.make_descriptor(node_id, pi++, scl.value, scale_names[ind++]);
  }

  dst[i++] = p.follow_keyboard.make_descriptor(node_id, pi++, p.follow_keyboard.value, "follow_keyboard");
}

GaussDistributedPitches1::Params::Params() {
  for (int i = 0; i < num_lobes; i++) {
    using Mu = std::remove_reference_t<decltype(mus[i])>;
    using Float = std::remove_reference_t<decltype(sigmas[i])>;
    mus[i] = Mu(0);
    sigmas[i] = Float(0.0f);
    scales[i] = Float(1.0f);
  }
}

GROVE_NAMESPACE_END
