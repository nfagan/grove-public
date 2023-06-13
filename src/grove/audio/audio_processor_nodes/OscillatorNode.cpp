#include "OscillatorNode.hpp"
#include "../AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/audio/Transport.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

double f01_to_beat_div(float f) {
  if (f < 0.125f) {
    return 4.0;
  } else if (f < 0.25f) {
    return 2.0;
  } else if (f < 0.5f) {
    return 1.0;
  } else if (f < 0.625f) {
    return 0.5;
  } else if (f < 0.75f) {
    return 0.25;
  } else {
    return 0.125;
  }
}

} //  anon

OscillatorNode::OscillatorNode(uint32_t node_id, const AudioParameterSystem* param_sys,
                               const Transport* transport, int num_channels) :
  node_id{node_id}, oscillator{default_sample_rate(), 1.0},
  parameter_system{param_sys}, transport{transport}, num_channels{num_channels} {
  //
  oscillator.fill_sin();
}

OutputAudioPorts OscillatorNode::outputs() const {
  OutputAudioPorts result;
  for (int i = 0; i < num_channels; i++) {
    OutputAudioPort port(BufferDataType::Float, const_cast<OscillatorNode*>(this), i);
    result.push_back(port);
  }
  return result;
}

InputAudioPorts OscillatorNode::inputs() const {
  return {};
}

void OscillatorNode::process(const AudioProcessData& in,
                             const AudioProcessData& out,
                             AudioEvents*,
                             const AudioRenderInfo& info) {
  (void) in;
  assert(out.descriptors.size() == num_channels);
  assert(in.descriptors.empty());

  const auto num_descriptors = out.descriptors.size();
  oscillator.set_sample_rate(info.sample_rate);

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  const auto self_changes = param_changes.view_by_parent(node_id);
  const auto wf_changes = self_changes.view_by_parameter(0);
  const auto freq_changes = self_changes.view_by_parameter(1);
  const auto sync_changes = self_changes.view_by_parameter(2);

  AudioParameterChange wf_change;
  if (wf_changes.collapse_to_last_change(&wf_change)) {
    const int curr_wf_type = params.waveform.value;
    params.waveform.apply(wf_change);
    const int new_wf_type = params.waveform.evaluate();
    if (new_wf_type != curr_wf_type) {
      switch (new_wf_type) {
        case 0:
          oscillator.fill_sin();
          break;
        case 1:
          oscillator.fill_tri(4);
          break;
        case 2:
          oscillator.fill_square(4);
          break;
        default:
          assert(false);
      }
    }
  }

  AudioParameterChange sync_change;
  if (sync_changes.collapse_to_last_change(&sync_change)) {
    params.tempo_sync.apply(sync_change);
    (void) params.tempo_sync.evaluate();
  }

  AudioParameterChange freq_change;
  if (freq_changes.collapse_to_last_change(&freq_change)) {
    params.frequency.apply(freq_change);
  }

  const double bpm = transport->get_bpm();
  const auto tsig = reference_time_signature();
  const double num = tsig.numerator;
  const double bps = beats_per_sample_at_bpm(bpm, info.sample_rate, tsig);
  if (transport->render_is_playing()) {
    cursor = transport->render_get_cursor_location();
  }

  for (int i = 0; i < info.num_frames; i++) {
    const float f01 = params.frequency.evaluate();
    cursor.wrapped_add_beats(bps, num);
    oscillator.set_frequency(lerp(f01, 0.1f, 10.0f));

    float sample;
    if (params.tempo_sync.value == 0) {
      sample = oscillator.tick();
    } else {
      const auto f = float(1.0 / f01_to_beat_div(f01) * num);
      const auto phase = wrap_within_range(cursor.beat * f, num) / num;
      sample = oscillator.read(phase * double(osc::WaveTable::size));
    }

    for (int j = 0; j < num_descriptors; j++) {
      assert(out.descriptors[j].is_float());
      out.descriptors[j].write<float>(out.buffer.data, i, &sample);
    }
  }
}

void OscillatorNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& view) const {
  auto* dst = view.push(2);
  uint32_t p{};
  int i{};
  Params ps{};
  dst[i++] = ps.waveform.make_descriptor(node_id, p++, ps.waveform.value, "waveform");
  dst[i++] = ps.frequency.make_descriptor(node_id, p++, ps.frequency.value, "frequency");
//  dst[i++] = ps.tempo_sync.make_descriptor(node_id, p++, ps.tempo_sync.value, "tempo_sync");
}

GROVE_NAMESPACE_END
