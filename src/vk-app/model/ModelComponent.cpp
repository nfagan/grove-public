#include "ModelComponent.hpp"
#include "mesh.hpp"
#include "../cloud/distribute_points.hpp"
#include "../terrain/terrain.hpp"
#include "grove/env.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/random.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/logging.hpp"
#include "grove/load/image.hpp"
#include "grove/load/obj.hpp"
#include "grove/visual/Image.hpp"
#include <iostream>

#define MODEL_TYPE (0)

GROVE_NAMESPACE_BEGIN

namespace {

[[maybe_unused]] constexpr const char* logging_id() {
  return "ModelComponent";
}

std::string res_dir() {
  return std::string{GROVE_ASSET_DIR};
}

constexpr Vec3f debug_model_origin() {
  return {-32.0f, 0.0f, 0.0f};
}

Optional<Image<uint8_t>> maybe_load_image(const std::string& im_p) {
  bool success{};
  auto res = load_image(im_p.c_str(), &success, true);
  if (!success) {
    return NullOpt{};
  } else {
    return Optional<Image<uint8_t>>(std::move(res));
  }
}

Optional<obj::VertexData> maybe_load_obj_data(const std::string& model_p,
                                              const std::string& model_dir) {
  bool success{};
  auto obj_model = obj::load_simple(model_p.c_str(), model_dir.c_str(), &success);
  if (!success) {
    return NullOpt{};
  } else {
    return Optional<obj::VertexData>(std::move(obj_model));
  }
}

vk::SampledImageManager::ImageCreateInfo to_image_create_info(const Image<uint8_t>& im) {
  vk::SampledImageManager::ImageCreateInfo info{};
  info.sample_in_stages = {vk::PipelineStage::FragmentShader};
//  info.format = VK_FORMAT_R8G8B8A8_UNORM;
  info.format = VK_FORMAT_R8G8B8A8_SRGB;
  info.data = im.data.get();
  info.descriptor.channels = image::Channels::make_uint8n(4);
  info.descriptor.shape = image::Shape::make_2d(im.width, im.height);
  info.image_type = vk::SampledImageManager::ImageType::Image2D;
  return info;
}

bool find_pos_norm_uv_attr_inds(const obj::VertexData& vd, int* p, int* n, int* uv) {
  int* ptrs[3] = {p, n, uv};
  obj::AttributeType attr_types[3] = {
    obj::AttributeType::Position,
    obj::AttributeType::Normal,
    obj::AttributeType::TexCoord,
  };
  for (int i = 0; i < 3; i++) {
    if (auto ind = vd.find_attribute(attr_types[i])) {
      *ptrs[i] = ind.value();
    } else {
      return false;
    }
  }
  return true;
}

[[maybe_unused]]
void init_rocks(ModelComponent& component, const ModelComponent::InitInfo& info) {
  auto& static_materials = component.static_materials;
  auto& static_geometries = component.static_geometries;

  if (static_materials.size() <= 1 || static_geometries.empty()) {
    return;
  }

  const int num_elements = 4;

  constexpr int stack_size = 128;
  Temporary<Vec2f, stack_size> store_dst_ps;
  Temporary<bool, stack_size> store_accept_ps;

  auto* dst_ps = store_dst_ps.require(num_elements);
  auto* accept_ps = store_accept_ps.require(num_elements);
  float r = points::place_outside_radius_default_radius(num_elements, 0.9f);
  points::place_outside_radius<Vec2f, float, 2>(dst_ps, accept_ps, num_elements, r);

  auto geometry = static_geometries[0];
  auto mat0 = static_materials[0];
  auto mat1 = static_materials[1];

  const float world_r = Terrain::terrain_dim * 0.5f;
  for (int i = 0; i < num_elements; i++) {
    auto material = urand() > 0.5 ? mat1 : mat0;
    StaticModelRenderer::DrawableParams p{};

    auto dst_p = (dst_ps[i] * 2.0f - 1.0f) * 0.5f;
    auto pos = Vec3f{dst_p.x * world_r, 0.0f, dst_p.y * world_r};
    auto scale = Vec3f{lerp(urandf(), 2.5f, 5.0f)};

    pos.y = info.terrain.height_nearest_position_xz(pos) - scale.x * 0.25f;

    p.transform = make_translation_scale(pos, scale);
    auto drawable = info.renderer.add_drawable(
      info.add_resource_context, geometry, material, p);
    (void) drawable;
  }
}

} //  anon

ModelComponent::InitResult ModelComponent::initialize(const InitInfo& init_info) {
  ModelComponent::InitResult result{};
  parent_trans0 = init_info.transform_system.create(TRS<float>::identity());

  if (!initialize_geometries(init_info)) {
    return result;
  }
  if (!initialize_materials(init_info)) {
    return result;
  }

#if 0
  init_rocks(*this, init_info);
#endif

#if 0
  if (static_materials.size() > 1 && !static_geometries.empty()) {
    auto geometry = static_geometries[0];
    auto mat0 = static_materials[0];
#if MODEL_TYPE == 0
    auto mat1 = static_materials[1];

    {
      StaticModelRenderer::DrawableParams params0{};
      params0.transform = Mat4f{1.0f};
      auto drawable0 = init_info.renderer.add_drawable(
        init_info.add_resource_context,
        geometry,
        mat0,
        params0);
      if (drawable0) {
        auto pos = Vec3f{8.0f} + debug_model_origin();
        auto scale = Vec3f{4.0f};
        Model model{};
        model.entity = Entity::create();
        model.drawable = drawable0.value();
        model.transform = init_info.transform_system.create(
          TRS<float>::make_translation_scale(pos, scale));
        models[model.entity] = model;
      }
    }
    {
      StaticModelRenderer::DrawableParams params1{};
      params1.transform = Mat4f{1.0f};
      auto drawable1 = init_info.renderer.add_drawable(
        init_info.add_resource_context,
        geometry,
        mat1,
        params1);
      if (drawable1) {
        auto pos = Vec3f{8.0f, 8.0f, 32.0f} + debug_model_origin();
        auto scale = Vec3f{4.0f};
        Model model{};
        model.entity = Entity::create();
        model.drawable = drawable1.value();
        model.transform = init_info.transform_system.create(
          TRS<float>::make_translation_scale(pos, scale));
        models[model.entity] = model;
      }
    }
    Entity last_entity{};
    for (uint32_t i = 0; i < 4; i++) {
      auto material = urand() > 0.5 ? mat1 : mat0;
      StaticModelRenderer::DrawableParams p{};
      p.transform = Mat4f{1.0f};
      auto pos = Vec3f{urand_11f(), 0.0f, urand_11f()} * 16.0f + debug_model_origin();
      auto scale = Vec3f{1.0f};
      auto drawable = init_info.renderer.add_drawable(
        init_info.add_resource_context, geometry, material, p);
      if (drawable) {
        Model model{};
        model.entity = Entity::create();
        model.drawable = drawable.value();
        model.transform = init_info.transform_system.create(
          TRS<float>::make_translation_scale(pos, scale));
        if (i == 0) {
          model.transform->set_parent(parent_trans0);
        } else {
          model.transform->set_parent(models.at(last_entity).transform);
        }
        last_entity = model.entity;
        models[model.entity] = model;
      }
    }
#else
    StaticModelRenderer::DrawableParams params0{};
#if MODEL_TYPE == 1
    auto pos = Vec3f{8.0f} + Vec3f{0.0f, 5.0f, 0.0f};
    auto scale = Vec3f{0.5f};
    params0.transform = Mat4f{1.0f};
#else
    params0.transform = make_translation_scale(
      Vec3f{8.0f, 5.0f, 8.0f} + debug_model_origin(), Vec3f{2.0f});
#endif
    auto drawable0 = init_info.renderer.add_drawable(
      init_info.add_resource_context,
      geometry,
      mat0,
      params0);
    if (drawable0) {
      Model model{};
      model.entity = Entity::create();
      model.drawable = drawable0.value();
      model.transform = init_info.transform_system.create(
        TRS<float>::make_translation_scale(pos, scale));
//      model.transform->set_parent(parent_trans0);
      models[model.entity] = model;
    }
#endif
  }
#endif

  for (auto& [entity, model] : models) {
    if (!model.transform_editor.is_valid()) {
      ModifyTransformEditor mod;
      AddTransformEditor add_inst{};
      add_inst.at_offset = Vec3f{0.0f, 8.0f, 0.0f};
      add_inst.register_with = entity;
      add_inst.target = model.transform;
      mod.add_instance = add_inst;
      result.modify_transform_editor.push_back(mod);
    }
  }

  return result;
}

bool ModelComponent::initialize_geometries(const InitInfo& init_info) {
#if MODEL_TYPE == 0
  auto obj_model = maybe_load_obj_data(
    res_dir() + "/models/rock1/rock1-painted.obj",
    res_dir() + "/models/rock1");
#elif MODEL_TYPE == 1
  auto obj_model = maybe_load_obj_data(
    res_dir() + "/models/jade-toad/JadeToad.obj",
    res_dir() + "/models/jade-toad");
#else
  auto obj_model = maybe_load_obj_data(
    res_dir() + "/models/rock3/rock3-base.obj",
    res_dir() + "/models/rock3");
#endif
  if (!obj_model) {
    return false;
  }

  int pos_ind{};
  int norm_ind{};
  int uv_ind{};
  if (!find_pos_norm_uv_attr_inds(obj_model.value(), &pos_ind, &norm_ind, &uv_ind)) {
    return false;
  }

  auto buff_descrip = vertex_buffer_descriptor_from_obj_data(obj_model.value());
  auto geom_handle = init_info.renderer.add_geometry(
    init_info.add_resource_context,
    obj_model.value().packed_data.data(),
    buff_descrip,
    obj_model.value().packed_data.size() * sizeof(float),
    pos_ind,
    norm_ind,
    uv_ind);
  if (!geom_handle) {
    return false;
  }

  static_geometries.push_back(geom_handle.value());
  return true;
}

bool ModelComponent::initialize_materials(const InitInfo& init_info) {
  {
#if MODEL_TYPE == 0
    auto im_res = maybe_load_image(res_dir() + "/models/rock1/textures/rock2.png");
#elif MODEL_TYPE == 1
    auto im_res = maybe_load_image(res_dir() + "/models/jade-toad/textures/Toad_Base_color.png");
#else
    auto im_res = maybe_load_image(res_dir() + "/models/rock3/textures/Material_Base_color.png");
#endif
    if (!im_res || im_res.value().num_components_per_pixel != 4) {
      return false;
    }
    auto info = to_image_create_info(im_res.value());
    auto im_handle = init_info.sampled_image_manager.create_sync(info);
    if (im_handle) {
      auto mat_handle = init_info.renderer.add_texture_material(
        init_info.add_resource_context, im_handle.value());
      if (!mat_handle) {
        return false;
      } else {
        static_materials.push_back(mat_handle.value());
      }
    } else {
      return false;
    }
  }
  {
    auto im_res = maybe_load_image(res_dir() + "/models/rock1/textures/rock1.png");
    if (!im_res || im_res.value().num_components_per_pixel != 4) {
      return false;
    }
    auto info = to_image_create_info(im_res.value());
    auto im_handle = init_info.sampled_image_manager.create_sync(info);
    if (im_handle) {
      auto mat_handle = init_info.renderer.add_texture_material(
        init_info.add_resource_context, im_handle.value());
      if (!mat_handle) {
        return false;
      } else {
        static_materials.push_back(mat_handle.value());
      }
    } else {
      return false;
    }
  }
  return true;
}

ModelComponent::UpdateResult ModelComponent::update(const UpdateInfo& info) {
  UpdateResult result{};
  {
    auto delta = float(timer.delta().count());
    parent_trans0->set(
      TRS<float>::make_translation(debug_model_origin() + Vec3f{std::sin(delta), 0.0f, 0.0f}));
  }
  for (auto& [_, model] : models) {
    StaticModelRenderer::DrawableParams params{};
    params.transform = to_mat4(model.transform->get_current());
    info.model_renderer.set_params(model.drawable, params);
  }
  return result;
}

void ModelComponent::register_transform_editor(Entity entity,
                                               editor::TransformEditorHandle handle) {
  models.at(entity).transform_editor = handle;
}

GROVE_NAMESPACE_END
