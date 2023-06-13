#include "SoilComponent.hpp"
#include "../generative/slime_mold.hpp"
#include "../imgui/SoilGUI.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

image::Descriptor make_soil_image_descriptor(int num_channels) {
  constexpr int tex_dim = gen::SlimeMoldConfig::texture_dim;
  return image::Descriptor{
    image::Shape::make_2d(tex_dim, tex_dim),
    image::Channels::make_floatn(num_channels)
  };
}

void overlay_player_position(void* data,
                             const image::Descriptor& desc,
                             const Soil& soil,
                             const Vec2f& p_xz,
                             float world_radius) {
  GROVE_ASSERT(desc.channels.has_single_channel_type(IntegralType::Float));
  auto p01 = soil.to_position01(p_xz);
  auto r01 = soil.to_length01(world_radius);
  auto* really_float = (float*) data;
  Vec3f val{1.0f};
  int tex_dim = desc.shape.width;
  int tex_chans = desc.channels.num_channels;
  gen::add_value(really_float, tex_dim, tex_chans, p01, r01, val);
}

} //  anon

void SoilComponent::initialize(const InitInfo& info) {
  if constexpr (gen::SlimeMoldConfig::num_texture_channels <= 4) {
    constexpr int tex_channels = 4;
    vk::DynamicSampledImageManager::ImageCreateInfo debug_im_create_info{};
    debug_im_create_info.image_type = vk::DynamicSampledImageManager::ImageType::Image2D;
    debug_im_create_info.descriptor = make_soil_image_descriptor(tex_channels);
    debug_im_create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
    if (auto res = info.image_manager.create_sync(
      info.image_create_context, debug_im_create_info)) {
      debug_image_handle = res.value();
    }
  }
}

SoilComponent::UpdateResult SoilComponent::update(const UpdateInfo& info) {
  UpdateResult result{};
  if (params.enabled) {
    if (!params.initialized) {
      soil.initialize();
      params.initialized = true;
    }
    soil.update();
  }
  if (debug_image_handle && params.draw_debug_image && params.initialized) {
    info.image_manager.set_data_from_contiguous_subset(
      debug_image_handle.value(),
      soil.read_image_data(),
      make_soil_image_descriptor(gen::SlimeMoldConfig::num_texture_channels));
    if (params.overlay_player_position) {
      info.image_manager.modify_data(
        debug_image_handle.value(),
        [&](void* data, const image::Descriptor& desc) {
          overlay_player_position(data, desc, soil, info.debug_position_xz, params.overlay_radius);
          return true;
        });
    }
    result.show_debug_image = debug_image_handle.value();
    result.debug_image_params.scale = Vec2f{0.75f};
    result.debug_image_params.translation = Vec2f{};
    result.debug_image_params.min_alpha = 1.0f;
  }
  return result;
}

void SoilComponent::on_gui_update(const SoilGUIUpdateResult& res) {
  if (res.enabled) {
    params.enabled = res.enabled.value();
  }
  if (res.draw_texture) {
    params.draw_debug_image = res.draw_texture.value();
  }
  if (res.overlay_player_position) {
    params.overlay_player_position = res.overlay_player_position.value();
  }
  if (res.overlay_radius) {
    params.overlay_radius = res.overlay_radius.value();
  }
  auto* config = soil.get_config();
  if (res.circular_world) {
    config->circular_world = res.circular_world.value();
  }
  if (res.decay) {
    config->decay = res.decay.value();
  }
  if (res.diffuse_speed) {
    config->diffuse_speed = res.diffuse_speed.value();
  }
  if (res.diffuse_enabled) {
    config->diffuse_enabled = res.diffuse_enabled.value();
  }
  if (res.allow_perturb_event) {
    config->allow_perturb_event = res.allow_perturb_event.value();
  }
  if (res.time_scale) {
    config->time_scale = res.time_scale.value();
  }
  if (res.only_right_turns) {
    config->only_right_turns = res.only_right_turns.value();
  }
  if (res.turn_speed_power) {
    soil.set_particle_turn_speed_power(res.turn_speed_power.value());
  }
  if (res.speed_power) {
    soil.set_particle_speed_power(res.speed_power.value());
  }
  if (res.only_right_turns) {
    soil.set_particle_use_only_right_turns(res.only_right_turns.value());
  }
}

GROVE_NAMESPACE_END
