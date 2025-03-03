#pragma once

#include "../render/ProceduralFlowerStemRenderer.hpp"
#include "../render/PointBufferRenderer.hpp"
#include "../render/render_ornamental_foliage_data.hpp"
#include "../audio_core/SimpleAudioNodePlacement.hpp"
#include "../imgui/ProceduralFlowerGUI.hpp"
#include "ProceduralFlowerBenderInstrument.hpp"
#include "ProceduralFlowerOrnamentParticles.hpp"
#include "petal.hpp"
#include "../procedural_flower/geometry.hpp"
#include "../procedural_tree/components.hpp"
#include "../procedural_tree/sync_growth.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

class Terrain;
class Transport;
class AudioObservation;
class SimpleAudioNodePlacement;

class ProceduralFlowerComponent {
  friend class ProceduralFlowerGUI;
public:
  struct Stem {
    tree::TreeNodeStore nodes;
    tree::SpawnInternodeParams spawn_params;
    tree::DistributeBudQParams bud_q_params;
    Optional<ProceduralFlowerStemRenderer::DrawableHandle> drawable;
    Vec3f color{};
    bool can_grow{true};
    bool finished_growing{};
    int last_num_internodes{-1};
    int max_num_internodes{};
  };

  struct AlphaTestPetalMaterialParams {
    int texture_layer;
    Vec3<uint8_t> color0;
    Vec3<uint8_t> color1;
    Vec3<uint8_t> color2;
    Vec3<uint8_t> color3;
  };

  using MakePetalShapeParams = std::function<petal::ShapeParams(float, float)>;

  struct StaticOrnamentParams {
    float min_scale{};
    float max_scale{};
  };

  struct PatchParams {
    float radius;
    int count;
  };

  struct Ornament {
    MakePetalShapeParams shape{};
    AlphaTestPetalMaterialParams alpha_test_petal_material_params{};
    float growth_frac{};
    float scale{1.0f};
    float uv_scale{1.0f};
    float death_frac{};
    bool place_along_medial_axis{};
    float tip_offset{};
    float growth_incr_randomness{};
    StaticOrnamentParams static_params{};
    Optional<foliage::OrnamentalFoliageInstanceHandle> foliage_instance_handle;
  };

  struct Flower {
    DynamicArray<Ornament, 2> ornaments;
    Stopwatch state_timer;
    bool ornaments_can_grow{};
    bool finished_render_growing{};
    bool finished_ornament_dying{};
    bool finished_ornament_dispersal{};
  };

  struct SpawnPollenParticle {
    Vec3f position{};
  };

  struct InitInfo {
    int num_material1_alpha_test_texture_layers;
    const Transport* transport;
    AudioNodeStorage& node_storage;
    AudioObservation& audio_observation;
    SimpleAudioNodePlacement& node_placement;
    Optional<vk::PointBufferRenderer::DrawableHandle> octree_point_drawable;
    const Terrain& terrain;
  };

  struct InitResult {
    SimpleAudioNodePlacement::CreateNodeResult pending_placement;
    Optional<Bounds3f> insert_audio_node_bounds_into_accel;
  };

  struct UpdatePointBuffer {
    vk::PointBufferRenderer::DrawableHandle handle;
    std::vector<Vec3f> points;
  };

  struct UpdateInfo {
    foliage::OrnamentalFoliageData* ornamental_foliage_data;
    const ProceduralFlowerStemRenderer::AddResourceContext& stem_context;
    ProceduralFlowerStemRenderer& stem_renderer;
    const Terrain& terrain;
    const SpatiallyVaryingWind& wind;
    double real_dt;
    const Vec3f& cursor_tform_position;
  };

  struct UpdateResult {
    DynamicArray<SpawnPollenParticle, 2> spawn_pollen_particles;
    Optional<UpdatePointBuffer> update_debug_attraction_points;
    Optional<vk::PointBufferRenderer::DrawableHandle> toggle_debug_attraction_points_drawable;
    int num_ornaments_finished_growing;
  };

  struct MakeStemParams {
    Vec3f origin;
    Vec3f color;
    int max_num_lateral_axes;
    int max_num_internodes;
    float scale;
    std::function<std::vector<Vec3f>(const Vec3f&, float)> make_attraction_points;
  };

  struct PendingNewPlant {
    MakeStemParams make_stem_params{};
    DynamicArray<Ornament, 2> ornaments;
  };

public:
  InitResult initialize(const InitInfo& init_info);
  UpdateResult update(const UpdateInfo& update_info);
  void add_patch(const Vec2f& pos_xz, const Optional<PatchParams>& patch_params);
  void add_patch_at_cursor_position();
  void add_patches_around_world() {
    params.need_add_patches_around_world = true;
  }
  void on_gui_update(const ProceduralFlowerGUI::UpdateResult& update_res);

//private:
  struct Params {
    Optional<bool> toggle_render_attraction_points;
    bool render_attraction_points{};
    float growth_time_limit_seconds{10.0e-3f};
    bool need_update_debug_octree{};
    bool need_reload_petal_program{};
    float axis_growth_incr{0.005f};
    float ornament_growth_incr{0.001f};
    float ornament_particles_dt_scale{1.0f};
    bool alpha_test_enabled{true};
    bool death_enabled{};
    float patch_radius{16.0f};
    int patch_size{8};
    float flower_stem_scale{2.25f};
    float flower_radius_power{5.0f};
    float flower_radius_scale{1.0f};
    float flower_radius_randomness{};
    float flower_radius_power_randomness{};
    bool randomize_flower_radius_power{};
    bool randomize_flower_radius_scale{};
    float patch_position_radius{};
    bool allow_bush{true};
    bool disable_alpha_test_ornaments{};
    bool need_add_patch_at_cursor{};
    bool need_add_patches_around_world{};
  };

  using StemRenderGrowthContexts =
    std::unordered_map<tree::TreeID, tree::RenderAxisGrowthContext, tree::TreeID::Hash>;
  using StemRenderDeathContexts =
    std::unordered_map<tree::TreeID, tree::RenderAxisDeathContext, tree::TreeID::Hash>;

  void update_ornament_particles(const UpdateInfo& update_info, UpdateResult& out);
  void update_bender_instrument(const UpdateInfo& update_info, UpdateResult& out);
  void update_growth(const UpdateInfo& update_info);
  void update_stem_axis_growth(const UpdateInfo& update_info);
  int update_ornament_growth(const UpdateInfo& update_info);
  void update_ornament_death(const UpdateInfo& update_info);
  void update_ornament_dispersal(const UpdateInfo& update_info);
  void update_debug_attraction_points_drawable(const UpdateInfo&, UpdateResult& out);
  void update_stem_axis_death(const UpdateInfo& update_info);
  void apply_growth_death_fraction(Ornament& ornament, const UpdateInfo& update_info);
  void maybe_make_plants(const UpdateInfo& update_info);
  void on_growth_cycle_start(const UpdateInfo& info);
  void on_growth_cycle_end(const UpdateInfo& info);
  void add_procedural_ornament(const UpdateInfo& info,
                               Ornament& ornament,
                               const std::vector<const tree::Internode*>& internodes,
                               const Bounds3f& node_aabb);

  bool should_start_growing() const;
  bool should_stop_growing() const;

  Stem* find_stem_by_id(tree::TreeID id);
  Flower* find_flower(tree::TreeID id);
  const Flower* find_flower(tree::TreeID id) const;

//private:
  std::vector<Stem> stems;
  std::unordered_map<tree::TreeID, Flower, tree::TreeID::Hash> flowers;
  tree::AttractionPoints attraction_points;
  tree::GrowthCycleContext stem_growth_cycle_context;
  StemRenderGrowthContexts stem_render_growth_contexts;
  StemRenderDeathContexts stem_render_death_contexts;
  Optional<vk::PointBufferRenderer::DrawableHandle> debug_attraction_points_drawable;
  Optional<tree::TreeID> selected_flower;
  int num_alpha_test_texture_layers{};

  bool growing{};
  Params params{};

  DynamicArray<PendingNewPlant, 2> pending_new_plants;
  ProceduralFlowerOrnamentParticles ornament_particles;

  ProceduralFlowerBenderInstrument bender_instrument;

  Stopwatch render_clock;
};

}