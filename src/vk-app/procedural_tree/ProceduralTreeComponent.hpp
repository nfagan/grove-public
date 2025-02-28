#pragma once

#include "audio_nodes.hpp"
#include "../render/PointBufferRenderer.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "../imgui/ProceduralTreeGUI.hpp"
#include "tree_system.hpp"
#include "render_tree_system.hpp"
#include "growth_system.hpp"
#include "tree_message_system.hpp"
#include "vine_system.hpp"
#include "message_particles.hpp"
#include "../transform/transform_system.hpp"
#include "components.hpp"
#include "ProceduralTreeInstrument.hpp"
#include "../audio_core/UIAudioParameterManager.hpp"
#include "../audio_core/AudioConnectionManager.hpp"
#include "../procedural_flower/ProceduralFlowerOrnamentParticles.hpp"
#include "../particle/pollen_particle.hpp"
#include "grove/audio/AudioParameterWriteAccess.hpp"
#include "grove/common/Stopwatch.hpp"
#include "grove/input/KeyTrigger.hpp"

namespace grove {

class Camera;
class Terrain;
class AudioObservation;
class AudioBuffers;
class SpatiallyVaryingWind;
class Soil;

namespace season {
struct StatusAndEvents;
}

class ProceduralTreeComponent {
public:
  friend class ProceduralTreeGUI;

  struct InitInfo {
    transform::TransformInstance* place_tree_tform_instance;
    tree::TreeSystem* tree_system;
    tree::GrowthSystem2* growth_system;
    UIAudioParameterManager& ui_parameter_manager;
    AudioParameterSystem* parameter_system;
    const Keyboard& keyboard;
    int initial_num_trees;
  };

  struct InitResult {
    Optional<KeyTrigger::Listener> key_listener;
  };

  struct BeginUpdateInfo {
    audio::NodeSignalValueSystem* node_signal_value_system;
    bool instrument_control_by_environment;
    float bpm11;
    tree::TreeSystem* tree_system;
  };

  enum class TreeState : uint8_t {
    Idle = 0,
    PendingPrepareToGrow,
    Growing,
    RenderGrowing,
    Pruning,
    RenderDying,
    PendingDeletion
  };

  enum class TreePhase : uint8_t {
    Idle = 0,
    AwaitingFinishGrowth,
    AwaitingInitialDrawableCreation,
    AwaitingFinishRenderGrowth,
    GrowingLeaves,
    PruningLeaves,
    PruningInternodes,
    AwaitingPrunedDrawableCreation,
    UnpruningLeaves,
    EvaluatingPrune,
    AwaitingFinishRenderDeath
  };

  struct UpdateInfo {
    tree::TreeSystem* tree_system;
    tree::RenderTreeSystem* render_tree_system;
    tree::GrowthSystem2* growth_system;
    tree::TreeMessageSystem* tree_message_system;
    tree::VineSystem* vine_system;
    bounds::BoundsSystem* bounds_system;
    bounds::AccelInstanceHandle insert_into_accel;
    const Camera& camera;
    const Terrain& terrain;
    const Soil& soil;
    double real_dt;
    const PollenParticles::UpdateResult& pollen_update_res;
    const SpatiallyVaryingWind& wind;
    AudioNodeStorage& node_storage;
    AudioObservation& audio_observation;
    const AudioScale& audio_scale;
    const AudioConnectionManager::UpdateResult& audio_connection_update_result;
    UIAudioParameterManager& ui_parameter_manager;
    AudioParameterSystem* parameter_system;
    const season::StatusAndEvents& season_status;
  };

  struct MakeOrnamentalFoliage {
    Vec2f position;
  };

  struct SpawnPollenParticle {
    Vec3f position;
    bool enable_tree_spawn;
  };

  struct SoilDeposit {
    Vec2f position;
    float radius;
    Vec3f amount;
  };

  using PendingPortPlacement = ProceduralTreeAudioNodes::PendingPortPlacement;

  struct UpdateResult {
    std::vector<std::unique_ptr<PendingPortPlacement>> pending_placement;
    DynamicArray<ProceduralTreeAudioNodes::ReleaseParameterWrite, 2> release_parameter_writes;
    DynamicArray<ProceduralTreeAudioNodes::NodeToDelete, 2> nodes_to_delete;
    DynamicArray<MakeOrnamentalFoliage, 2> new_ornamental_foliage_patches;
    std::vector<SpawnPollenParticle> spawn_pollen_particles;
    DynamicArray<SoilDeposit, 2> soil_deposits;
    Optional<vk::PointBufferRenderer::DrawableHandle> toggle_debug_attraction_points_drawable;
    int num_leaves_finished_growing{};
    int num_began_dying{};
  };

  struct BranchSwellInfo {
    float swell_fraction{};
    float swell_incr{0.005f};
    bool triggered_swell{};
    int8_t sense_channel_index{};
    int8_t deposit_channel_index{};
  };

  struct TreeMeta {
    bool finished_render_growth{};
    bool finished_growing{};
    bool can_trigger_pollen_spawn{};
    bool triggered_pollen_spawn{};
    bool dying{};
    bool deserialized{};
    TreeState tree_state{};
    TreePhase tree_phase{};
    Stopwatch pollen_spawn_timer;
    Stopwatch alive_timer;
    float canonical_leaf_scale{};
    float time_to_season_transition{};
    std::unique_ptr<PendingPortPlacement> ports_pending_placement{};
    BranchSwellInfo swell_info{};
    bool need_start_dying{};
    int resource_spiral_handle_indices[4]{};
  };

  template <typename T>
  using TreeIDMap = std::unordered_map<tree::TreeID, T, tree::TreeID::Hash>;

  struct Tree {
  public:
    bool is_fully_grown() const {
      return meta.finished_render_growth;
    }
    void set_need_start_dying() {
      meta.need_start_dying = true;
    }

  public:
    Vec3f origin;
    float unhealthiness;
    tree::TreeInstanceHandle instance;
    tree::RenderTreeInstanceHandle render_instance;
    Optional<tree::VineInstanceHandle> vine_instance;
    TreeMeta meta;
  };

  struct ActiveMessage {
    msg::MessageID message_id;
  };

  using Trees = TreeIDMap<Tree>;

public:
  [[nodiscard]] InitResult initialize(const InitInfo& init_info);
  void begin_update(const BeginUpdateInfo& info);
  UpdateResult update(const UpdateInfo& update_info);
  void on_gui_update(const ProceduralTreeGUI::GUIUpdateResult& update_res);
  void register_pollen_particle(PollenParticleID id);
  void set_need_start_dying(tree::TreeID tree);
  void set_healthiness(tree::TreeID tree, float h01);
  Vec3f centroid_of_tree_origins() const;
  const Trees* maybe_read_trees() const;
  ArrayView<const tree::TreeID> read_newly_created() const;
  ArrayView<const tree::TreeID> read_newly_destroyed() const;
  void create_tree(bool at_tform_pos);
  void create_tree_patches();
  bool any_growing() const;
  Vec3f get_place_tform_translation() const;
  int num_trees_in_world() const {
    return int(trees.size());
  }
  void evaluate_audio_node_isolator_update_result(
    tree::RenderTreeSystem* render_tree_system,
    AudioNodeStorage::NodeID newly_activated, AudioNodeStorage::NodeID newly_deactivated);

public:
  struct PendingNewTree {
    Vec3f position;
    std::unique_ptr<tree::TreeNodeStore> deserialized{};
  };

  struct TreePendingRemoval {
    tree::TreeID id;
  };

  using BranchRenderGrowthContexts = TreeIDMap<tree::RenderAxisGrowthContext>;
  using BranchRenderDeathContexts = TreeIDMap<tree::RenderAxisDeathContext>;

public:
  tree::GrowthContextHandle growth_context;
  Trees trees;
  Optional<tree::TreeID> selected_tree;

  DynamicArray<tree::TreeID, 16> newly_created;
  DynamicArray<tree::TreeID, 16> newly_destroyed;

  bounds::AccessorID bounds_accessor_id{bounds::AccessorID::create()};

  AudioParameterWriterID parameter_writer_id{};
  ProceduralTreeInstrument tree_instrument;
  ProceduralTreeAudioNodes audio_nodes;

  std::unordered_set<PollenParticleID, PollenParticleID::Hash> active_pollen_particles;

  DynamicArray<ActiveMessage, 8> active_messages;
  tree::MessageParticles message_particles;

  transform::TransformInstance* place_tree_tform_instance;
  bool need_grow{};
  bool remake_branch_program{};
  bool remake_flower_leaves_program{};
  bool tree_spawn_enabled{};
  DynamicArray<PendingNewTree, 2> pending_new_trees;
  DynamicArray<TreePendingRemoval, 2> trees_pending_removal;
  bool render_node_skeleton{};
  bool render_axis_root_info{};
  bool render_branches{true};
  bool render_leaves{true};
  bool render_branch_aabb{};
  bool use_high_lod_leaf_shadow{};
  bool auto_high_lod_leaf_shadow{};
  float noise_texture_sample_scale{0.5f};
  bool axis_growth_by_signal{true};
  bool leaf_growth_by_signal{};
  bool can_trigger_death{};
//  float axis_growth_incr{0.075f};
  float axis_growth_incr{0.075f * 0.5f};
  float axis_death_incr{0.05f};
  float leaf_growth_incr{0.01f};
  bool wind_influence_enabled{true};
//  Vec2f wind_strength_limits{0.03f, 0.22f};
  Vec2f wind_strength_limits{0.03f, 0.1f};
  int attraction_points_type{};
  int spawn_params_type{};
  bool is_pine{};
  int foliage_leaves_type{};
  float growth_fraction{1.0f};
  float signal_axis_growth_incr_scale{0.1f * 0.5f};
  float signal_leaf_growth_incr_scale{0.01f};
//  float wind_fast_osc_amplitude_scale{10.0f};
  float proc_wind_fast_osc_amplitude_scale{2.0f};
  float static_wind_fast_osc_amplitude_scale{0.05f};
  int num_trees_manually_add{1};
  Vec3f default_new_tree_origin{32.0f, 0.0f, -32.0f};
  float new_tree_origin_span{32.0f};
  bool add_flower_patch_after_growing{true};
  bool hide_foliage_drawable_components{};
  Vec3f deserialized_tree_translation{};
  bool need_add_tree_at_tform_position{};
  bool use_static_leaves{};
  bool use_hemisphere_color_image{true};
  bool randomize_hemisphere_color_images{};
  bool randomize_static_or_proc_leaves{};
  bool disable_static_leaves{};
  bool disable_foliage_components{};
  bool always_small_proc_leaves{};
  bool disable_restricting_tree_origins_to_within_world_bound{};
  int next_audio_node_type{};
  float resource_spiral_global_particle_velocity{6.0f};
  float resource_spiral_global_particle_theta{pif() * 0.25f};
  Stopwatch season_transition_timer{};
  bool grow_vines_by_signal{true};
  bool need_reset_tform_position{};
  Optional<AudioNodeStorage::NodeID> isolated_audio_node;
  Optional<std::string> serialize_selected_to_file_path;
  Optional<int> prune_selected_axis_index;
};

}