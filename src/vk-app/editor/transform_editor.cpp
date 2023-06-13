#include "transform_editor.hpp"
#include "grove/common/common.hpp"
#include "grove/math/string_cast.hpp"
#include "grove/math/intersect.hpp"

GROVE_NAMESPACE_BEGIN

using namespace editor;

namespace {

Optional<TransformEditor::ToChange>
update_translation_instance(TransformEditorHandle handle,
                            TransformEditor::Instance& instance,
                            const Vec3f& position,
                            const Ray& cursor_ray) {
  uint32_t plane_ind = instance.active_plane_index.unwrap();
  auto ind = min_dimension(instance.drawable_info[plane_ind].scale);
  Vec4f p{};
  p[ind] = 1.0f;
  p[3] = -position[ind];
  float t;
  if (ray_plane_intersect(cursor_ray, p, &t)) {
    auto hit_pos = cursor_ray(t);
    auto delta = hit_pos - instance.last_cursor_position;
    auto tmp = instance.last_cursor_position[ind];
    instance.last_cursor_position = hit_pos;
    instance.last_cursor_position[ind] = tmp;
    delta[ind] = 0.0f;

    if (!instance.first_hit) {
      TransformEditor::ToChange to_change;
      to_change.target = handle;
      to_change.translation = delta;
      return Optional<TransformEditor::ToChange>(to_change);
    } else {
      instance.first_hit = false;
    }
  }

  return NullOpt{};
}

} //  anon

void editor::TransformEditor::update(const UpdateInfo& info) {
  for (auto& [handle, inst] : instances) {
    auto& trs = inst.transform->get_current();
    const auto& pos = trs.translation;
    if (active_instances.count(handle)) {
      if (info.cursor_down) {
        if (auto trans = update_translation_instance(handle, inst, pos, info.cursor_ray)) {
          auto* target = inst.transform->get_parent();
          auto src = target->get_source();
          src.translation += trans.value().translation.value();
          target->set(src);
        }
      } else {
        active_instances.erase(handle);
        inst.active_plane_index = NullOpt{};
      }
    }
    for (uint32_t i = 0; i < inst.num_selectables; i++) {
      auto scale = inst.drawable_info[i].scale;
      inst.cursor_monitorables[i]->set_bounds(
        Bounds3f{pos - scale, pos + scale});
    }
  }
}

TransformEditorHandle
editor::TransformEditor::create_instance(transform::TransformInstance* target,
                                         transform::TransformSystem& transform_system,
                                         cursor::Monitor& cursor_monitor,
                                         const Vec3f& position) {
  Instance instance{};
  instance.transform = transform_system.create(TRS<float>::make_translation(position));
  instance.transform->set_parent(target);
  TransformEditorHandle handle{next_transform_editor_id++};
  for (int i = 0; i < 3; i++) {
    Vec3f scale{2.0f};
    scale[i] = 0.1f;

    InstanceDrawableInfo drawable_info{};
    drawable_info.color = Vec3f{1.0f};
    drawable_info.color[i] = 0.0f;
    drawable_info.scale = scale;
    instance.drawable_info[instance.num_selectables] = drawable_info;

    Bounds3f bounds{position - scale, position + scale};
    auto monitorable = cursor_monitor.create_monitorable(
      cursor::SelectionLayer{0},
      bounds,
      nullptr,
      [this, handle](const cursor::StateChangeInfo& info) {
        if (info.event.is_down()) {
          if (active_instances.empty()) {
            active_instances.insert(handle);
            auto& inst = instances.at(handle);
            inst.first_hit = true;
            for (uint32_t i = 0; i < inst.num_selectables; i++) {
              if (inst.cursor_monitorables[i]->get_id() == info.id) {
                inst.active_plane_index = i;
              }
            }
          }
        }
      });
    instance.cursor_monitorables[instance.num_selectables] = monitorable;
    instance.num_selectables++;
  }

  instances[handle] = instance;
  return handle;
}

void editor::TransformEditor::destroy_instance(TransformEditorHandle handle,
                                               cursor::Monitor& cursor_monitor) {
  if (auto it = instances.find(handle); it != instances.end()) {
    auto& inst = it->second;
    for (uint32_t i = 0; i < inst.num_selectables; i++) {
      cursor_monitor.destroy_monitorable(inst.cursor_monitorables[i]);
    }
    instances.erase(it);
  } else {
    assert(false);
  }
}

void editor::TransformEditor::set_color(TransformEditorHandle handle, const Vec3f& color) {
  if (auto it = instances.find(handle); it != instances.end()) {
    auto& inst = it->second;
    for (uint32_t i = 0; i < inst.num_selectables; i++) {
      inst.drawable_info[i].color = color;
    }
  }
}

void editor::TransformEditor::set_disabled(TransformEditorHandle handle, bool disable) {
  if (auto it = instances.find(handle); it != instances.end()) {
    auto& inst = it->second;
    for (uint32_t i = 0; i < inst.num_selectables; i++) {
      inst.drawable_info[i].disabled = disable;
    }
  }
}

GROVE_NAMESPACE_END
