#include "DestinationNode.hpp"
#include "../AudioParameterSystem.hpp"
#include "../AudioEventSystem.hpp"
#include "../fdft.hpp"
#include "../dft.hpp"
#include "grove/common/common.hpp"
#include "grove/common/logging.hpp"
#include <cstring>

GROVE_NAMESPACE_BEGIN

namespace {

template <int DftSize>
bool mean_signal_amplitude(const AudioProcessBuffer& buff, const BufferChannelDescriptor& channel,
                           int num_frames, float* amp) {
  const int i0 = std::max(num_frames - DftSize, 0);
  const int i1 = std::min(i0 + DftSize, num_frames);

  if (i1 <= i0) {
    return false;
  }

  const int n = i1 - i0;
  if ((n & (n - 1)) != 0) {
    //  Expect power of two.
    return false;
  }

  assert(channel.is_float());
  float src_dft_samples[DftSize];
  float dst_dft_samples[DftSize * 2];

  for (int i = i0; i < i1; i++) {
    float v;
    channel.read(buff.data, i, &v);
    src_dft_samples[i - i0] = v;
  }

  fdft(dst_dft_samples, src_dft_samples, n);
  *amp = sum_complex_moduli(dst_dft_samples, n) / float(n);
  return true;
}

} //  anon

DestinationNode::DestinationNode(AudioParameterID node_id,
                                 const AudioParameterSystem* parameter_system, int num_channels) :
  node_id{node_id},
  parameter_system{parameter_system},
  out_samples(nullptr) {
  //
  for (int i = 0; i < num_channels; i++) {
    InputAudioPort input_port(BufferDataType::Float, this, i);
    input_ports.push_back(input_port);
  }
}

InputAudioPorts DestinationNode::inputs() const {
  return input_ports;
}

OutputAudioPorts DestinationNode::outputs() const {
  return {};
}

void DestinationNode::set_output_sample_buffer(Sample* out) {
  out_samples = out;
}

void DestinationNode::set_node_id(AudioParameterID id) {
  assert(node_id == 0 || node_id == id);
  node_id = id;
}

void DestinationNode::process(const AudioProcessData& in,
                              const AudioProcessData&,
                              AudioEvents*,
                              const AudioRenderInfo& info) {
  assert(out_samples);
  auto num_descriptors =
    std::min(int(in.descriptors.size()), info.num_channels);

  const auto& param_changes = param_system::render_read_changes(parameter_system);
  auto all_changes = param_changes.view_by_parent(node_id);

  auto gain_changes = all_changes.view_by_parameter(0);
  int gain_ind{};

  for (int i = 0; i < info.num_frames; i++) {
    maybe_apply_change(gain_changes, gain_ind, gain, i);
    const auto gain_val = gain.evaluate();

    for (int j = 0; j < num_descriptors; j++) {
      assert(in.descriptors[j].is_float());
      const int out_index = i * info.num_channels + j;
      float current = out_samples[out_index];

      float v;
      in.descriptors[j].read(in.buffer.data, i, &v);

      float res = v * gain_val + current;
      std::memcpy(&out_samples[out_index], &res, sizeof(float));
    }
  }

  maybe_record_data(in, info);

  if (num_descriptors > 0 && info.num_frames > 0) {
    float v{};
    if (mean_signal_amplitude<64>(in.buffer, in.descriptors[0], info.num_frames, &v)) {
      const float min_db = -50.0f;
      const float max_db = 12.0f;
      v = (clamp(float(amplitude_to_db(v)), min_db, max_db) - min_db) / (max_db - min_db);

      auto stream = audio_event_system::default_event_stream();
      const int write_frame = info.num_frames - 1;
      //  @NOTE: hardcoded parameter index `1`, see below
      auto evt = make_monitorable_parameter_audio_event(
        {node_id, 1}, make_float_parameter_value(v), write_frame, 0);
      (void) audio_event_system::render_push_event(stream, evt);
    }
  }
}

void DestinationNode::maybe_record_data(const AudioProcessData& in, const AudioRenderInfo& info) {
  int num_pending_record = pending_record_info.size();
  for (int i = 0; i < num_pending_record; i++) {
    active_record_info = pending_record_info.read();
  }

  auto* recorder = active_record_info.recorder;
  auto stream_handle = active_record_info.stream_handle;
  DynamicArray<int, 2> descriptor_indices;

  const auto can_record = [&]() -> bool {
    if (!recorder || !stream_handle.is_valid() || !recorder->is_recording(stream_handle)) {
      return false;
    }
    for (int i = 0; i < int(in.descriptors.size()); i++) {
      if (in.descriptors[i].is_float()) {
        descriptor_indices.push_back(i);
      }
    }
    return descriptor_indices.size() == 2;
  }();

  if (can_record) {
    auto descr_inds_view = make_iterator_array_view<int>(descriptor_indices);
    if (!recorder->accumulate(stream_handle, in, descr_inds_view, info)) {
      GROVE_LOG_ERROR_CAPTURE_META("Failed to record audio data.", "DestinationNode");
    }
  }
}

bool DestinationNode::set_record_info(const RecordInfo& info) {
  return pending_record_info.maybe_write(info);
}

void DestinationNode::parameter_descriptors(TemporaryViewStack<AudioParameterDescriptor>& mem) const {
  auto* dst = mem.push(2);
  dst[0] = gain.make_descriptor(node_id, 0, default_gain, "gain");

  auto monitor_flags = AudioParameterDescriptor::Flags::marked_monitorable_non_editable();
  dst[1] = signal_repr.make_descriptor(node_id, 1, 0.0f, "signal_representation", monitor_flags);
}

GROVE_NAMESPACE_END
