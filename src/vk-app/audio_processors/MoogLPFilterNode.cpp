#include "MoogLPFilterNode.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

MoogLPFilterNode::MoogLPFilterNode(AudioParameterID node_id,
                                   const AudioParameterSystem* parameter_system) :
  node_id{node_id},
  parameter_system{parameter_system} {
  //
}

InputAudioPorts MoogLPFilterNode::inputs() const {
  InputAudioPorts input_ports;

  int pi{};
  for (int i = 0; i < 2; i++) {
    input_ports.push_back(InputAudioPort{
      BufferDataType::Float, const_cast<MoogLPFilterNode*>(this), pi++});
  }

  auto opt_flag = AudioPort::Flags::marked_optional();
  input_ports.push_back(InputAudioPort{
    BufferDataType::Float, const_cast<MoogLPFilterNode*>(this), pi++, opt_flag
  });

  return input_ports;
}

OutputAudioPorts MoogLPFilterNode::outputs() const {
  OutputAudioPorts output_ports;
  for (int i = 0; i < 2; i++) {
    output_ports.push_back(OutputAudioPort{
      BufferDataType::Float, const_cast<MoogLPFilterNode*>(this), i});
  }
  return output_ports;
}

void MoogLPFilterNode::process(
  const AudioProcessData& in, const AudioProcessData& out, AudioEvents*,
  const AudioRenderInfo& info) {
  //
  assert(in.descriptors.size() == 3);

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  const auto self_changes = param_changes.view_by_parent(node_id);
  const auto cutoff_changes = self_changes.view_by_parameter(0);
  const auto res_changes = self_changes.view_by_parameter(1);

  int cutoff_change_ind{};
  int res_change_ind{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(cutoff_changes, cutoff_change_ind, cutoff, i);
    maybe_apply_change(res_changes, res_change_ind, resonance, i);

    float cut = cutoff.evaluate();
    const float res = resonance.evaluate();

    if (!in.descriptors[2].is_missing()) {
      float mod_11{};
      in.descriptors[2].read(in.buffer.data, i, &mod_11);
      mod_11 = clamp(mod_11, -1.0f, 1.0f) * 2.5e3f;
      cut = clamp(cut + mod_11, CutoffLimits::min, CutoffLimits::max);
    }

    for (auto& st : state) {
      st.update(info.sample_rate, cut, res);
    }

    for (int j = 0; j < 2; j++) {
      float in_v;
      in.descriptors[j].read(in.buffer.data, i, &in_v);

      float out_v = state[j].tick(in_v);
      out.descriptors[j].write(out.buffer.data, i, &out_v);
    }
  }
}

void MoogLPFilterNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  const int np = 2;
  auto* dst = mem.push(np);
  int i{};
  uint32_t p{};
  //  cutoff
  dst[i++] = cutoff.make_descriptor(node_id, p++, cutoff_default, "cutoff");
  //  res
  dst[i++] = resonance.make_descriptor(node_id, p++, resonance_default, "resonance");
}

GROVE_NAMESPACE_END
