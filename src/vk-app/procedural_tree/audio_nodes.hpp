#pragma once

#include "../audio_core/AudioNodeStorage.hpp"
#include "ProceduralTreeInstrument.hpp"
#include "components.hpp"
#include "grove/audio/AudioParameterWriteAccess.hpp"

namespace grove {

struct AudioParameterSystem;
class AudioObservation;
class ProceduralTreeInstrument;

namespace audio {
struct NodeSignalValueSystem;
}

struct ProceduralTreeAudioNodes {
public:
  template <typename T>
  using TreeIDMap = std::unordered_map<tree::TreeID, T, tree::TreeID::Hash>;

  struct ReleaseParameterWrite {
    AudioParameterWriterID writer_id{};
    AudioParameterIDs param_ids{};
  };

  struct NodeToDelete {
    AudioNodeStorage::NodeID id;
    bool remove_placed_node;
  };

  struct PendingPortPlacement {
    AudioNodeStorage::NodeID node_id;
    AudioNodeStorage::PortInfoForNode port_info;
    Vec3f position;
    float y_offset;
  };

  struct DelayNodeInfo {
    AudioNodeStorage::NodeID node_id{};
    Vec3f position{};
    Optional<AudioParameterIDs> chorus_mix_param_ids;
    Optional<AudioParameterIDs> noise_mix_param_ids;
    Optional<float> signal_value;
  };

  struct EnvelopeNodeInfo {
    AudioNodeStorage::NodeID node_id{};
    Vec3f position{};
    Optional<float> signal_value;
    Optional<AudioParameterDescriptor> amp_mod_descriptor;
  };

  struct ReverbNodeInfo {
    AudioNodeStorage::NodeID node_id{};
    Vec3f position{};
    Optional<float> signal_value;
    Optional<AudioParameterIDs> mix_param_ids;
    Optional<AudioParameterIDs> fb_param_ids;
    Optional<AudioParameterIDs> fixed_osc_mix_param_ids;
  };

  struct TriggeredOscNodeInfo {
    AudioNodeStorage::NodeID node_id{};
    Vec3f position{};
    Optional<AudioParameterIDs> signal_param_ids;
    Optional<AudioParameterIDs> monitor_note_number_param_ids;
    Optional<AudioParameterDescriptor> semitone_offset_desc;
    Optional<float> signal_value;
    Optional<float> note_number_value;
  };

  struct Context {
    AudioParameterWriterID parameter_writer;
    AudioNodeStorage& node_storage;
    AudioParameterSystem& parameter_system;
    AudioObservation& audio_observation;
    const AudioScale& audio_scale;
    ProceduralTreeInstrument& tree_instrument;
  };

  struct RemoveNodeResult {
    ArrayView<const ReleaseParameterWrite> release_parameter_writes;
    NodeToDelete pending_deletion;
  };

public:
  void gather_parameter_ids(const Context& context);
  void process_monitorable_changes(
    audio::NodeSignalValueSystem* node_signal_value_system,
    const ArrayView<const ProceduralTreeInstrument::ObservableChange>& changes);
  Optional<float> get_signal_value(tree::TreeID id) const;
  Optional<float> get_triggered_osc_note_number_value(tree::TreeID id) const;

  PendingPortPlacement create_delay_node(const Context& context, tree::TreeID tree_id,
                                         const Vec3f& pos, float port_y_offset);

  PendingPortPlacement create_envelope_node(const Context& context, tree::TreeID tree_id,
                                            const Vec3f& pos, float port_y_offset);

  PendingPortPlacement create_reverb_node(const Context& context, tree::TreeID tree_id,
                                          const Vec3f& pos, float port_y_offset);

  PendingPortPlacement create_triggered_osc_node(const Context& context, tree::TreeID tree_id,
                                                 const Vec3f& pos, float port_y_offset);

  RemoveNodeResult destroy_node(const Context& context, tree::TreeID by_id, bool remove_placed_node);

public:
  TreeIDMap<DelayNodeInfo> delay_nodes;
  TreeIDMap<EnvelopeNodeInfo> envelope_nodes;
  TreeIDMap<ReverbNodeInfo> reverb_nodes;
  TreeIDMap<TriggeredOscNodeInfo> triggered_osc_nodes;
  std::unordered_map<AudioNodeStorage::NodeID, tree::TreeID> audio_node_id_to_tree_id;
  DynamicArray<ReleaseParameterWrite, 8> released_parameter_writes;
};

}
