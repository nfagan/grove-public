#pragma once

#include "grove/common/Optional.hpp"

namespace grove {

class RenderComponent;
class ShadowComponent;

namespace vk {
struct GraphicsContext;
}

namespace gfx {
struct Context;
}

namespace tree {
struct RenderTreeSystem;
}

struct GraphicsGUIUpdateResult {
  struct StaticModelParams {
    bool remake_programs{};
    Optional<bool> disable_simple_shape_renderer;
  };

  struct ProceduralTreeParams {
    Optional<bool> disabled;
    bool remake_programs{};
  };

  struct ProceduralTreeRootsParams {
    bool remake_programs{};
  };

  struct FoliageParams {
    bool remake_programs{};
    Optional<bool> disable_pcf;
    Optional<bool> disable_alpha_image;
    Optional<bool> disable_color_mix;
    Optional<bool> enable_fixed_shadow;
    Optional<bool> enable_gpu_driven_foliage_rendering;
    Optional<bool> enable_gpu_driven;
    Optional<bool> gpu_driven_use_tiny_array_images;
    Optional<bool> gpu_driven_use_alpha_to_coverage;
    Optional<bool> gpu_driven_cpu_occlusion_enabled;
    Optional<int> gpu_driven_max_shadow_cascade_index;
  };

  struct ProceduralFlowerOrnamentParams {
    bool remake_procedural_ornament_programs{};
    Optional<bool> render_static_drawables;
    Optional<bool> render_alpha_test_drawables;
    Optional<bool> use_high_lod_grid_geometry_buffers;
  };

  struct OrnamentalFoliageParams {
    Optional<bool> disable;
    Optional<bool> disable_stem;
  };

  struct ShadowComponentParams {
    Optional<float> projection_sign_y;
  };

  struct CloudParams {
    bool remake_programs{};
    Optional<bool> render_enabled;
  };

  struct ArchParams {
    Optional<bool> randomized_color;
    Optional<bool> hidden;
    bool remake_programs{};
  };

  struct GrassParams {
    Optional<bool> render_high_lod;
    Optional<bool> render_low_lod;
    Optional<bool> render_high_lod_post_pass;
    Optional<bool> pcf_enabled;
    Optional<float> max_diffuse;
    Optional<float> max_specular;
    Optional<bool> prefer_alt_color_image;
    bool remake_programs{};
  };

  struct TerrainParams {
    bool remake_programs{};
  };

  struct CullParams {
    Optional<float> far_plane_distance;
    Optional<bool> debug_draw;
  };

  ProceduralTreeParams proc_tree_params{};
  ProceduralTreeRootsParams proc_tree_roots_params{};
  FoliageParams foliage_params{};
  ShadowComponentParams shadow_component_params{};
  CloudParams cloud_params{};
  StaticModelParams static_model_params{};
  ArchParams arch_params{};
  GrassParams grass_params{};
  TerrainParams terrain_params{};
  CullParams cull_params{};
  OrnamentalFoliageParams ornamental_foliage_params{};
  bool close{};
};

class GraphicsGUI {
public:
  GraphicsGUIUpdateResult render(vk::GraphicsContext& graphics_context,
                                 const gfx::Context& opaque_graphics_context,
                                 RenderComponent& render_component,
                                 const ShadowComponent& shadow_component,
                                 tree::RenderTreeSystem& render_tree_system);

  bool show_context_stats{};
  bool show_foliage_stats{};
  int foliage_query_pool_size{64};
};

}