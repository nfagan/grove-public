#include "Reverb1Node.hpp"
#include "grove/audio/AudioParameterSystem.hpp"
#include "grove/common/common.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"

GROVE_NAMESPACE_BEGIN

Reverb1Node::Reverb1Node(AudioParameterID node_id, int param_offset,
                         const AudioParameterSystem* parameter_system,
                         Layout layout, const Params& params) :
  node_id{node_id},
  parameter_offset{param_offset},
  parameter_system{parameter_system},
  layout{layout},
  params{params},
  mix{params.default_mix},
  fdn_feedback{params.default_fdn_feedback} {
  //
  if (layout == Layout::Sample2) {
    input_ports.push_back(InputAudioPort{BufferDataType::Sample2, this, 0});
    output_ports.push_back(OutputAudioPort{BufferDataType::Sample2, this, 0});

  } else if (layout == Layout::TwoChannelFloat) {
    for (int i = 0; i < 2; i++) {
      input_ports.push_back(InputAudioPort{BufferDataType::Float, this, i});
      output_ports.push_back(OutputAudioPort{BufferDataType::Float, this, i});
    }

  } else {
    assert(false);
  }
}

InputAudioPorts Reverb1Node::inputs() const {
  return input_ports;
}

OutputAudioPorts Reverb1Node::outputs() const {
  return output_ports;
}

void Reverb1Node::process(const AudioProcessData& in,
                          const AudioProcessData& out,
                          AudioEvents*,
                          const AudioRenderInfo& info) {
  GROVE_ASSERT_MATCHES_PORT_LAYOUTS(in, input_ports, out, output_ports);

  const auto changes = param_system::render_read_changes(parameter_system).view_by_parent(node_id);
  const auto mix_changes = changes.view_by_parameter(parameter_offset);
  const auto fb_changes = changes.view_by_parameter(parameter_offset + 1);

  int fb_change_index{};
  int mix_change_index{};

  if (last_sample_rate != info.sample_rate) {
    reverb.set_sample_rate(info.sample_rate);
    last_sample_rate = info.sample_rate;
  }

  if (layout == Layout::Sample2) {
    auto in0 = in.descriptors[0];
    auto out0 = out.descriptors[0];
    assert(in0.is_sample2() && out0.is_sample2());

    for (int i = 0; i < info.num_frames; i++) {
      Sample2 src;
      in0.read(in.buffer.data, i, &src);

      maybe_apply_change(fb_changes, fb_change_index, fdn_feedback, i);
      maybe_apply_change(mix_changes, mix_change_index, mix, i);

      auto feedback_value = fdn_feedback.evaluate();
      auto mix_value = mix.evaluate();

      Sample2 dest = reverb.tick(src, info.sample_rate, feedback_value, mix_value);
      out0.write(out.buffer.data, i, &dest);
    }

  } else if (layout == Layout::TwoChannelFloat) {
    assert(in.descriptors.size() == 2 && out.descriptors.size() == 2);

    for (int i = 0; i < info.num_frames; i++) {
      Sample2 src;

      for (int j = 0; j < 2; j++) {
        assert(in.descriptors[j].is_float());
        float s;
        in.descriptors[j].read(in.buffer.data, i, &s);
        src.samples[j] = s;
      }

      maybe_apply_change(fb_changes, fb_change_index, fdn_feedback, i);
      maybe_apply_change(mix_changes, mix_change_index, mix, i);

      auto feedback_value = fdn_feedback.evaluate();
      auto mix_value = mix.evaluate();

      Sample2 dest = reverb.tick(src, info.sample_rate, feedback_value, mix_value);

      for (int j = 0; j < 2; j++) {
        assert(out.descriptors[j].is_float());
        float o = dest.samples[j];
        out.descriptors[j].write(out.buffer.data, i, &o);
      }
    }

  } else {
    assert(false);
  }
}

void Reverb1Node::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  const int np = 2;
  auto* dst = mem.push(np);
  uint32_t p{};
  int i{};
  dst[i++] = mix.make_descriptor(
    node_id, parameter_offset + (p++), params.default_mix, "mix");
  dst[i++] = fdn_feedback.make_descriptor(
    node_id, parameter_offset + (p++), params.default_fdn_feedback, "feedback");
}

GROVE_NAMESPACE_END