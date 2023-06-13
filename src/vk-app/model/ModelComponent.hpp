#pragma once

#include "../render/StaticModelRenderer.hpp"
#include "../render/SimpleShapeRenderer.hpp"
#include "../render/SimpleShapePools.hpp"
#include "../editor/render.hpp"
#include "../editor/transform_editor.hpp"
#include "../transform/transform_system.hpp"
#include "../editor/entity.hpp"
#include "grove/common/Stopwatch.hpp"

namespace grove {

class Terrain;

class ModelComponent {
public:
  struct AddTransformEditor {
    Vec3f at_offset;
    Entity register_with;
    transform::TransformInstance* target;
  };
  struct RemoveTransformEditor {
    editor::TransformEditorHandle handle;
  };
  struct ModifyTransformEditor {
    Optional<AddTransformEditor> add_instance;
    Optional<RemoveTransformEditor> remove_instance;
  };
  struct InitInfo {
    StaticModelRenderer& renderer;
    vk::SampledImageManager& sampled_image_manager;
    const StaticModelRenderer::AddResourceContext& add_resource_context;
    transform::TransformSystem& transform_system;
    const Terrain& terrain;
  };
  struct InitResult {
    std::vector<ModifyTransformEditor> modify_transform_editor;
  };
  struct UpdateInfo {
    const editor::UIRenderer::DrawContext& ui_draw_context;
    editor::UIRenderer& ui_renderer;
    StaticModelRenderer& model_renderer;
  };
  struct UpdateResult {
    //
  };
  struct Model {
    Entity entity{};
    StaticModelRenderer::DrawableHandle drawable{};
    editor::TransformEditorHandle transform_editor{};
    transform::TransformInstance* transform{};
  };

public:
  [[nodiscard]] InitResult initialize(const InitInfo& init_info);
  [[nodiscard]] UpdateResult update(const UpdateInfo& info);
  void register_transform_editor(Entity entity, editor::TransformEditorHandle handle);

private:
  bool initialize_geometries(const InitInfo& init_info);
  bool initialize_materials(const InitInfo& init_info);

public:
  std::vector<StaticModelRenderer::GeometryHandle> static_geometries;
  std::vector<StaticModelRenderer::MaterialHandle> static_materials;
  std::unordered_map<Entity, Model, Entity::Hash> models;
  transform::TransformInstance* parent_trans0;
  Stopwatch timer;
};

}