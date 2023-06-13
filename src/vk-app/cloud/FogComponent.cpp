#include "FogComponent.hpp"
#include "grove/common/common.hpp"
#include "../weather/common.hpp"
#include "../wind/SpatiallyVaryingWind.hpp"
#include "../imgui/FogGUI.hpp"
#include "grove/load/image.hpp"
#include "grove/visual/Image.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/math/random.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using UpdateInfo = FogComponent::UpdateInfo;

float wrap01(float v) {
  while (v < 0.0f) {
    v += 1.0f;
  }
  while (v >= 1.0f) {
    v -= 1.0f;
  }
  return v;
}

void wrap01(Vec3f* v) {
  v->x = wrap01(v->x);
  v->y = wrap01(v->y);
  v->z = wrap01(v->z);
}

float choose_mist_alive_time() {
  return 48.0f + 6.0f * urand_11f();
}

constexpr Vec3f base_mist_uvw_scale() {
  return Vec3f{0.125f, 0.125f * 0.5f, 0.125f};
}

Vec3f choose_mist_uvw_scale() {
  Vec3f base = base_mist_uvw_scale();
  Vec3f rand = base * 0.5f * Vec3f{urandf(), urandf(), urandf()};
  return base - rand;
}

CloudRenderer::BillboardDrawableParams make_mist_billboard_drawable_params() {
  CloudRenderer::BillboardDrawableParams result{};
  float scl = 20.0f + urand_11f() * 4.0f;
  result.opacity_scale = 0.0f;
  result.depth_test_enabled = true;
  result.scale = Vec3f{scl, scl, 1.0f};
  return result;
}

void update_billboard_uvw(CloudRenderer::BillboardDrawableParams& params, const Vec3f& uvw_scale,
                          float dt, const SpatiallyVaryingWind& wind, const Camera& camera) {
  auto wind_dir = wind.get_dominant_wind_direction();
  auto cam_right = camera.get_right();
  Vec2f cam_right_xz = normalize(Vec2f{cam_right.x, cam_right.z});
  auto wind_f = -dot(wind_dir, cam_right_xz);

  auto uvw_incr = Vec3f{wind_f * dt, -dt, dt} * uvw_scale;
  params.uvw_offset += uvw_incr;
}

void initialize_transient_mist_elements(fog::TransientMistElement* elements, int num_elements) {
  fog::distribute_transient_mist_elements(elements, num_elements);
  for (int i = 0; i < num_elements; i++) {
    auto& el = elements[i];
    el.total_time = choose_mist_alive_time();
    el.elapsed_time = el.total_time;
  }
}

void initialize_transient_mist(FogComponent& component, const UpdateInfo& info) {
  for (int i = 0; i < 8; i++) {
    FogComponent::TransientMistDrawable mist{};
    mist.drawable_params = make_mist_billboard_drawable_params();
    if (auto drawable = info.cloud_renderer.create_billboard_drawable(
      info.renderer_context, component.fog_image.value(), mist.drawable_params)) {
      mist.drawable = drawable.value();
      mist.uvw_scale = choose_mist_uvw_scale();
      component.transient_mist_drawables[component.num_transient_mists++] = mist;
    }
  }
  initialize_transient_mist_elements(
    component.transient_mist_elements.data(),
    component.num_transient_mists);
}

void update_transient_mist(FogComponent& component, const UpdateInfo& info) {
  auto cam_pos = info.camera.get_position();
  auto cam_right = info.camera.get_right();
  auto cam_forward = info.camera.get_front();

  fog::TransientMistTickParams params{};
  params.camera_position = &cam_pos;
  params.camera_right = &cam_right;
  params.camera_forward = &cam_forward;
  params.terrain = &info.terrain;
  params.y_offset = 2.0f;
  params.real_dt = float(info.real_dt);
  params.grid_size = 32.0f;
  params.dist_begin_attenuation = 64.0f;
  params.camera_front_distance_limits = Vec2f{96.0f, 96.0f + 32.0f};
  params.camera_right_distance_limits = Vec2f{-32.0f, 32.0f};

  fog::tick_transient_mist(
    component.transient_mist_elements.data(),
    component.num_transient_mists,
    params);

  for (int i = 0; i < component.num_transient_mists; i++) {
    auto& mist = component.transient_mist_drawables[i];
    auto& mist_el = component.transient_mist_elements[i];
    mist.drawable_params.translation = mist_el.position;
    mist.drawable_params.opacity_scale = mist_el.opacity;
    update_billboard_uvw(
      mist.drawable_params, mist.uvw_scale, float(info.real_dt), info.wind, info.camera);
    info.cloud_renderer.set_drawable_params(mist.drawable, mist.drawable_params);
  }
}

float density_scale_from_weather_status(const weather::Status& status, float max_scale) {
  if (status.current == weather::State::Overcast) {
    return (1.0f - status.frac_next) * max_scale;
  } else {
    return std::pow(status.frac_next, 4.0f) * max_scale;
  }
}

} //  anon

FogComponent::InitResult FogComponent::initialize(const InitInfo& info) {
  InitResult result{};

  int num_cells_yx = 16;
  int num_cells_z = 8;
  int grid_cell_px = 16;
  worley_noise_params.num_cells[0] = num_cells_yx;
  worley_noise_params.num_cells[1] = num_cells_yx;
  worley_noise_params.num_cells[2] = num_cells_z;
  worley_noise_params.cell_sizes_px[0] = grid_cell_px;
  worley_noise_params.cell_sizes_px[1] = grid_cell_px;
  worley_noise_params.cell_sizes_px[2] = grid_cell_px;
  worley_noise_params.invert = true;

#if 0
  {
    auto tex_file = "/textures/clouds/volume_wisp256x256.dat";
    auto tex_file_path = std::string{GROVE_PLAYGROUND_RES_DIR} + tex_file;
    bool success{};
    auto texture_data = read_3d_noise_texture(tex_file_path.c_str(), &success);
    if (success && !texture_data.empty()) {
      auto& ref = texture_data[0];
      cloud_data.data = pack_texture_layers(texture3_data_to_uint8(texture_data));
      cloud_data.desc = image::Descriptor{
        image::Shape::make_3d(ref.width, ref.height, int(texture_data.size())),
        image::Channels::make_uint8n(ref.num_components_per_pixel)
      };
    }
  }
#endif

  billboard_transform = info.transform_system.create(
    TRS<float>::make_translation_scale(Vec3f{32.0f, 64.0f, 32.0f}, Vec3f{32.0f}));
//  result.add_transform_editor.push_back(billboard_transform);

  debug_transform = info.transform_system.create(
    TRS<float>::make_translation_scale(Vec3f{32.0f}, Vec3f{32.0f}));
//  result.add_transform_editor.push_back(debug_transform);
  recompute_noise = true;
  make_fog = true;
  return result;
}

void FogComponent::set_common_fog_config(const UpdateInfo&) {
  manual_density_scale = false;
  wind_influence_enabled = true;
  wind_influence_scale = 0.25f;
  debug_drawable_params.density_scale = 2.0f;
  debug_drawable_params.depth_test_enable = true;
  debug_transform->set(
    TRS<float>::make_translation_scale(Vec3f{}, Vec3f{256.0f, 128.0f, 256.0f}));
}

void FogComponent::update(const UpdateInfo& info) {
  if (make_fog) {
    set_common_fog_config(info);
    make_fog = false;
  }

  if (!debug_fog_drawable) {
    if (fog_image_future && fog_image_future->is_ready()) {
      fog_image = fog_image_future->data;
      fog_image_future = nullptr;
    }
    if (fog_image) {
      debug_fog_drawable = info.cloud_renderer.create_volume_drawable(
        info.renderer_context,
        fog_image.value(),
        debug_drawable_params);
      if (debug_fog_drawable) {
        info.cloud_renderer.set_active(debug_fog_drawable.value(), true);
      }
    }
  }

  if (!debug_billboard_drawable) {
    if (fog_image) {
      debug_billboard_drawable = info.cloud_renderer.create_billboard_drawable(
        info.renderer_context,
        fog_image.value(),
        debug_billboard_params);
      info.cloud_renderer.set_active(debug_billboard_drawable.value(), false);
    }
  } else {
    auto& trs = billboard_transform->get_current();
    debug_billboard_params.translation = trs.translation;
    debug_billboard_params.scale = trs.scale;
    info.cloud_renderer.set_drawable_params(
      debug_billboard_drawable.value(), debug_billboard_params);
  }

  if (fog_image && !initialized_transient_mists) {
    initialize_transient_mist(*this, info);
    initialized_transient_mists = true;
  }

  update_transient_mist(*this, info);
  update_billboard_uvw(
    debug_billboard_params, base_mist_uvw_scale(), float(info.real_dt), info.wind, info.camera);

  if (awaiting_noise_result) {
    auto status = worley_noise_future.wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      awaiting_noise_result = false;
      fog_data = worley_noise_future.get();
    }
  } else if (recompute_noise && !fog_image_future) {
    auto noise_p = worley_noise_params;
    auto num_im_components = num_fog_image_components;
    worley_noise_future = std::async(std::launch::async, [noise_p, num_im_components]() {
      int px_dims[3];
      get_image_dims_px(noise_p, px_dims);
      size_t num_image_px = num_im_components * worley::get_image_size_px(px_dims);
      auto image_data = std::make_unique<uint8_t[]>(num_image_px);

      auto num_grid_px = get_sample_grid_size_px(noise_p);
      auto point_grid = std::make_unique<uint8_t[]>(num_grid_px);

      for (int i = 0; i < num_im_components; i++) {
        worley::generate_sample_grid<uint8_t>(num_grid_px, point_grid.get());
        worley::generate(
          noise_p,
          px_dims,
          point_grid.get(),
          image_data.get(),
          num_im_components,
          i);
      }

      WorleyNoiseFutureData result{};
      result.data = std::move(image_data);
      result.desc = image::Descriptor{
        image::Shape::make_3d(px_dims[1], px_dims[0], px_dims[2]),  //  @NOTE, width vs rows
        image::Channels::make_uint8n(num_im_components)
      };
      return result;
    });
    recompute_noise = false;
    awaiting_noise_result = true;
  }

  if (fog_data.data && !fog_image && !fog_image_future) {
    vk::DynamicSampledImageManager::ImageCreateInfo create_info{};
    create_info.data = fog_data.data.get();
    create_info.descriptor = fog_data.desc;
    create_info.int_conversion = IntConversion::UNorm;
    create_info.image_type = vk::DynamicSampledImageManager::ImageType::Image3D;
    create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};

    if (auto fut_handle = info.image_manager.create_async(info.image_context, create_info)) {
      fog_image_future = fut_handle.value();
    }
  }

  weather_driven_density_scale = density_scale_from_weather_status(
    info.weather_status, debug_drawable_params.density_scale);

  if (debug_fog_drawable) {
    if (wind_influence_enabled) {
      auto wind3 = Vec3f{info.wind_direction.x, 0.0f, info.wind_direction.y};
      float scale = wind_influence_scale * info.wind_force * float(info.real_dt);
      debug_drawable_params.uvw_offset -= wind3 * scale;
      wrap01(&debug_drawable_params.uvw_offset);
    }

    auto& trs = debug_transform->get_current();
    debug_drawable_params.translation = trs.translation;
    debug_drawable_params.scale = trs.scale;

    if (manual_density_scale) {
      info.cloud_renderer.set_drawable_params(
        debug_fog_drawable.value(), debug_drawable_params);
    } else {
      auto draw_params = debug_drawable_params;
      draw_params.density_scale = weather_driven_density_scale;
      info.cloud_renderer.set_drawable_params(debug_fog_drawable.value(), draw_params);
    }

    auto& render_params = info.cloud_renderer.get_render_params();
    render_params.cloud_color = fog_color;
  }
}

void FogComponent::on_gui_update(const FogGUIUpdateResult& res) {
  if (res.recompute_noise) {
    recompute_noise = true;
  }
  if (res.make_fog) {
    make_fog = true;
  }
  if (res.new_transform_source) {
    debug_transform->set(res.new_transform_source.value());
  }
  if (res.billboard_transform_source) {
    billboard_transform->set(res.billboard_transform_source.value());
  }
  if (res.depth_test_enabled) {
    debug_drawable_params.depth_test_enable = res.depth_test_enabled.value();
  }
  if (res.wind_influence_enabled) {
    wind_influence_enabled = res.wind_influence_enabled.value();
  }
  if (res.wind_influence_scale) {
    wind_influence_scale = res.wind_influence_scale.value();
  }
  if (res.uvw_offset) {
    debug_drawable_params.uvw_offset = res.uvw_offset.value();
  }
  if (res.uvw_scale) {
    debug_drawable_params.uvw_scale = res.uvw_scale.value();
  }
  if (res.color) {
    fog_color = res.color.value();
  }
  if (res.density) {
    debug_drawable_params.density_scale = res.density.value();
  }
  if (res.manual_density) {
    manual_density_scale = res.manual_density.value();
  }
  if (res.billboard_depth_test_enabled) {
    debug_billboard_params.depth_test_enabled = res.billboard_depth_test_enabled.value();
  }
  if (res.billboard_opacity_scale) {
    debug_billboard_params.opacity_scale = res.billboard_opacity_scale.value();
  }
}

GROVE_NAMESPACE_END
