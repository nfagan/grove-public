#include "WindComponent.hpp"
#include "grove/common/common.hpp"
#include "grove/common/profile.hpp"

GROVE_NAMESPACE_BEGIN

WindComponent::InitResult WindComponent::initialize(const InitInfo& info) {
  InitResult result{};
  wind_displacement.initialize(wind);
  {
    int tex_dim = wind_displacement.texture_dim();
    vk::DynamicSampledImageManager::ImageCreateInfo create_info{};
    create_info.sample_in_stages = {vk::PipelineStage::VertexShader};
    create_info.image_type = vk::DynamicSampledImageManager::ImageType::Image2D;
    create_info.descriptor = {
      image::Shape::make_2d(tex_dim, tex_dim),
      image::Channels::make_floatn(2)
    };
    if (auto handle = info.image_manager.create_sync(info.create_context, create_info)) {
      displacement_image_handle = handle.value();
      result.wind_displacement_image = handle.value();
    }
  }
  wind_particles.initialize(1000);
  return result;
}

void WindComponent::update(const UpdateInfo& info) {
  auto profiler = GROVE_PROFILE_SCOPE_TIC_TOC("WindComponent/update");
  double real_dt = std::min(0.25, info.real_dt);
  const Vec2f cam_pos_xz{info.camera_position.x, info.camera_position.z};

  wind.update(real_dt);
  wind_displacement.update(wind, real_dt);
  wind_particles.update(info.camera_position, wind.wind_force(cam_pos_xz));

  if (displacement_image_handle) {
    const auto& displace = wind_displacement.read_displacement();
    info.image_manager.set_data(displacement_image_handle.value(), displace.get());
  }
}

Vec2f WindComponent::approx_displacement_limits() const {
  return {
    wind_displacement.approx_idle_magnitude(),
    wind_displacement.approx_gust_magnitude()
  };
}

Vec2f WindComponent::render_axis_strength_limits() const {
  return Vec2f{0.03f, 0.1f};
}

GROVE_NAMESPACE_END
