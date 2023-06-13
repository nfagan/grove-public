#include "TerrainComponent.hpp"
#include "weather.hpp"
#include "grove/env.hpp"
#include "grove/common/common.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Image.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

std::string res_dir() {
  return GROVE_ASSET_DIR;
}

[[maybe_unused]] std::string full_heightmap_path(const char* as_input) {
  return res_dir() + "/heightmaps/edited/" + as_input;
}

vk::SampledImageManager::ImageCreateInfo make_color_image_create_info(const Image<uint8_t>& im) {
  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.image_type = vk::SampledImageManager::ImageType::Image2D;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  create_info.descriptor = {
    image::Shape::make_2d(im.width, im.height),
    image::Channels::make_uint8n(im.num_components_per_pixel)
  };
  create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  create_info.data = im.data.get();
  return create_info;
}

} //  anon

void TerrainComponent::initialize(const InitInfo& info) {
  terrain.initialize();
#if 1
  auto height_map_p = full_heightmap_path("beach.dat");
  (void) terrain.load_height_map(height_map_p.c_str());
#endif

  {
    //  Height map
    vk::DynamicSampledImageManager::ImageCreateInfo create_info{};
    create_info.image_type = vk::DynamicSampledImageManager::ImageType::Image2D;
    create_info.sample_in_stages = {vk::PipelineStage::VertexShader};
    create_info.descriptor = {
      image::Shape::make_2d(Terrain::texture_dim, Terrain::texture_dim),
      image::Channels::make_floatn(1)
    };
    create_info.data = terrain.read_height_map_data().get();
    auto im_handle = info.dynamic_image_manager.create_sync(
      info.create_dynamic_image_context, create_info);
    if (im_handle) {
      info.terrain_renderer.set_height_map_image(im_handle.value());
      info.grass_renderer.set_height_map_image(im_handle.value());
    }
  }

  {
    //  Terrain color
    bool success{};
    auto tex_p = res_dir() + "/textures/grass/terrain-grass3-tile.png";
    auto im = load_image(tex_p.c_str(), &success);
    if (success && im.num_components_per_pixel == 4) {
      if (auto handle = info.image_manager.create_sync(make_color_image_create_info(im))) {
        info.terrain_renderer.set_color_image(handle.value());
        info.grass_renderer.set_terrain_color_image(handle.value());
      }
    }
  }

#if 1
  set_new_material_image_file_path("/textures/grass/new_terrain_experiment.png", true);
#endif
}

TerrainComponent::UpdateResult TerrainComponent::update(const UpdateInfo& info) {
  UpdateResult result{};

  auto render_params = weather::terrain_render_params_from_status(info.weather_status);
  result.min_shadow = render_params.min_shadow;
  result.global_color_scale = render_params.global_color_scale;
  result.frac_global_color_scale = render_params.frac_global_color_scale;

  if (new_material_image_file_path) {
    auto tex_p = std::move(new_material_image_file_path.value());
    bool success{};
    auto im = load_image(tex_p.c_str(), &success);
    if (success && im.num_components_per_pixel == 4) {
      auto create_info = make_color_image_create_info(im);
      if (info.image_manager.require_sync(&new_material_image_handle, create_info)) {
        result.new_material_image_handle = new_material_image_handle;
      }
    }
    new_material_image_file_path = NullOpt{};
  }

  return result;
}

void TerrainComponent::set_new_material_image_file_path(const std::string& p, bool prepend_asset_dir) {
  if (prepend_asset_dir) {
    new_material_image_file_path = res_dir() + p;
  } else {
    new_material_image_file_path = p;
  }
}

Bounds3f TerrainComponent::world_aabb() const {
  return Bounds3f{
    Vec3f{-Terrain::terrain_dim * 0.5f},
    Vec3f{Terrain::terrain_dim * 0.5f}
  };
}

GROVE_NAMESPACE_END
