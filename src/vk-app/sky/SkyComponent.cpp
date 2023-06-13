#include "SkyComponent.hpp"
#include "../imgui/SkyGUI.hpp"
#include "grove/common/common.hpp"
#include "../weather/common.hpp"
#include "weather.hpp"
#include "sun.hpp"

#define PREFER_ALT_SUN (1)

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateInfo = SkyComponent::UpdateInfo;

bool need_reevaluate_gradient(SkyGradient::Params& params,
                              const SkyProperties& props) {
  auto midpoints = props.gradient_mid_points.data.read_or_default(Vec3f{});

  auto color0 = props.y0_color.data.read_or_default(Vec3f{});
  auto color1 = props.y1_color.data.read_or_default(Vec3f{});
  auto color2 = props.y2_color.data.read_or_default(Vec3f{});
  auto color3 = props.y3_color.data.read_or_default(Vec3f{});

  bool modified = false;
  if (params.y0_color != color0) {
    modified = true;
    params.y0_color = color0;
  }
  if (params.y1_color != color1) {
    modified = true;
    params.y1_color = color1;
  }
  if (params.y2_color != color2) {
    modified = true;
    params.y2_color = color2;
  }
  if (params.y3_color != color3) {
    modified = true;
    params.y3_color = color3;
  }
  if (params.y1 != midpoints.x) {
    modified = true;
    params.y1 = midpoints.x;
  }
  if (params.y2 != midpoints.y) {
    modified = true;
    params.y2 = midpoints.y;
  }

  return modified;
}

Optional<SkyGradient::Params> on_weather_status_change(SkyComponent& component,
                                                       const UpdateInfo& info) {
  Optional<SkyGradient::Params> new_params;

  auto& weather_status = info.weather_status;
  if (weather_status.current == weather::State::Sunny &&
      weather_status.next == weather::State::Overcast) {
    new_params = weather::sunny_to_overcast_gradient_params(weather_status.frac_next);
    component.sun.color = weather::sunny_to_overcast_sun_color(weather_status.frac_next);
    //
  } else if (weather_status.current == weather::State::Overcast &&
             weather_status.next == weather::State::Sunny) {
    new_params = weather::overcast_to_sunny_gradient_params(weather_status.frac_next);
    component.sun.color = weather::overcast_to_sunny_sun_color(weather_status.frac_next);
  } else {
    //  @TODO
    assert(false);
  }

  return new_params;
}

} //  anon

SkyComponent::UpdateResult SkyComponent::update(const UpdateInfo& info) {
  UpdateResult result{};
  if (sky_image_future && sky_image_future->is_ready()) {
    sky_image = sky_image_future->data;
    result.sky_image = sky_image_future->data;
    sky_image_future = nullptr;
  }

  if (need_set_default_sun) {
    sun = Sun{};
//    sun_position_theta_frac = 0.25;
//    sun_position_phi_radians = 0.0;
    need_set_default_sun = false;
  }

  if (use_sun_angles) {
    auto sun_dist = properties.sun_offset.read_or_default(128.0f);
    auto sun_pos = sun::compute_position(
      sun_position_theta_frac, sun_position_phi_radians, sun_dist);
    sun.position = to_vec3f(sun_pos);
  }

  if (info.weather_status.changed) {
    if (auto grad_params = on_weather_status_change(*this, info)) {
      gradient_params_from_weather = grad_params.value();
    }
  }

  auto* use_grad_params = weather_controls_sky_gradient ?
    &gradient_params_from_weather : &gradient_params_from_user;
  gradient.evaluate(*use_grad_params);
  if (sky_image) {
    info.image_manager.set_data(sky_image.value(), gradient.data.get());
  }

  return result;
}

void SkyComponent::initialize(const InitInfo& info) {
  if (need_reevaluate_gradient(gradient_params_from_user, properties)) {
    gradient.evaluate(gradient_params_from_user);
    gradient_params_from_weather = gradient_params_from_user;
  }

  {
    //  color image
    auto& grad_params = gradient_params_from_user;
    vk::DynamicSampledImageManager::ImageCreateInfo create_info{};
    create_info.image_type = vk::DynamicSampledImageManager::ImageType::Image2D;
    create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
    create_info.data = gradient.data.get();
    create_info.descriptor = {
      image::Shape::make_2d(grad_params.texture_size, grad_params.texture_size),
      image::Channels::make_floatn(4)
    };
    auto fut_handle = info.dynamic_image_manager.create_async(
      info.dynamic_image_create_context, create_info);
    if (fut_handle) {
      sky_image_future = std::move(fut_handle.value());
    }
  }

#if PREFER_ALT_SUN
  use_sun_angles = true;
  sun_position_theta_frac = 0.357f;
#endif
}

void SkyComponent::on_gui_update(const SkyGUIUpdateResult& res) {
  if (res.weather_controls_gradient) {
    weather_controls_sky_gradient = res.weather_controls_gradient.value();
  }
  if (res.sky_gradient_params) {
    gradient_params_from_user = res.sky_gradient_params.value();
  }
  if (res.use_default_sun) {
    need_set_default_sun = true;
  }
  if (res.use_sun_angles) {
    use_sun_angles = res.use_sun_angles.value();
    if (!use_sun_angles) {
      need_set_default_sun = true;
    }
  }
  if (res.sun_position_theta01) {
    sun_position_theta_frac = res.sun_position_theta01.value();
  }
  if (res.sun_position_phi_radians) {
    sun_position_phi_radians = res.sun_position_phi_radians.value();
  }
}

GROVE_NAMESPACE_END
