#include "DebugTuning.hpp"
#include "parameter.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/audio/AudioScaleSystem.hpp"
#include "grove/audio/oscillator.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

#if 0
//  12-tet
const double scale0[13]{
  1.000000000000000, 1.059463094359295, 1.122462048309373, 1.189207115002721, 1.259921049894873,
  1.334839854170034, 1.414213562373095, 1.498307076876682, 1.587401051968199, 1.681792830507429,
  1.781797436280679, 1.887748625363387, 2.000000000000000
};

//  pelog.scl
const double scale1[8]{
  1.000000000000000, 1.142857142857143, 1.200000000000000, 1.312500000000000, 1.500000000000000,
  1.600000000000000, 1.750000000000000, 2.000000000000000
};

double scale_to_rate_multiplier(const double* scale, int scale_size, int note_off) {
  assert(scale_size > 1 && scale[0] == 1.0);

  double rm = 1.0;
  double mi = scale[scale_size - 1];  //  max interval, e.g. 2/1

  while (note_off < 0) {
    note_off += scale_size - 1;
    rm /= mi;
  }

  while (note_off >= scale_size - 1) {
    note_off -= (scale_size - 1);
    rm *= mi;
  }

  assert(note_off >= 0 && note_off < scale_size);
  return rm * scale[note_off];
}
#endif

} //  anon

DebugTuning::DebugTuning(uint32_t node_id) :
  node_id{node_id} {
  //
  note_number = midi_note_number_a4();
}

InputAudioPorts DebugTuning::inputs() const {
  InputAudioPorts result;
  result.push_back(InputAudioPort{BufferDataType::MIDIMessage, const_cast<DebugTuning*>(this), 0});
  return result;
}

OutputAudioPorts DebugTuning::outputs() const {
  OutputAudioPorts result;
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<DebugTuning*>(this), 0});
  result.push_back(OutputAudioPort{BufferDataType::Float, const_cast<DebugTuning*>(this), 1});
  return result;
}

void DebugTuning::parameter_descriptors(TemporaryViewStack<grove::AudioParameterDescriptor>& mem) const {
  Params ps;
  uint32_t pi{};
  int di{};
  auto* dst = mem.push(Params::num_params);
  dst[di++] = ps.scale_frac.make_default_descriptor(node_id, pi++, "scale_frac");
}

void DebugTuning::process(
  const AudioProcessData& in, const AudioProcessData& out,
  AudioEvents*, const AudioRenderInfo& info) {
  //
  {
    auto* param_sys = param_system::get_global_audio_parameter_system();
    const auto& param_changes = param_system::render_read_changes(param_sys);

    auto self_changes = param_changes.view_by_parent(node_id);
    uint32_t pi{};
    check_apply_float_param(params.scale_frac, self_changes.view_by_parameter(pi++));
  }

  const auto* scale_sys = scale_system::get_global_audio_scale_system();

  for (int i = 0; i < info.num_frames; i++) {
    MIDIMessage message{};
    in.descriptors[0].read(in.buffer.data, i, &message);
    if (message.is_note_on()) {
      note_number = message.note_number();
    }

#if 0
    const float frac_s0 = params.scale_frac.evaluate();
    const int ni = int(note_number) - int(midi_note_number_a4());
    const double rm0 = scale_to_rate_multiplier(scale0, sizeof(scale0)/sizeof(double), ni);
    const double rm1 = scale_to_rate_multiplier(scale1, sizeof(scale1)/sizeof(double), ni);
    const double osc_freq = lerp(double(frac_s0), rm0, rm1) * frequency_a4();
#else
    const double osc_freq = scale_system::render_get_frequency(scale_sys, note_number, i);
#endif

    auto s = float(osc::Sin::tick(info.sample_rate, &osc_phase, osc_freq));
    const float g = 0.5f;
    s *= g;

    out.descriptors[0].write(out.buffer.data, i, &s);
    out.descriptors[1].write(out.buffer.data, i, &s);
  }
}

GROVE_NAMESPACE_END
