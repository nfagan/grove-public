#include "debug.hpp"
#include "../transform/transform_system.hpp"
#include "../editor/editor.hpp"
#include "../render/debug_draw.hpp"
#include "grove/common/common.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/random.hpp"
#include <vector>
#include <unordered_map>

GROVE_NAMESPACE_BEGIN

namespace {

using namespace bounds;
using namespace bounds::debug;

struct DebugInstance {
  AccelInstanceHandle accel;
  AccessorID accessor_id;
  OBB3f intersect_bounds;
  transform::TransformInstance* transform;
  editor::TransformEditorHandle transform_editor;
  bool draw_intersecting;
  Optional<bool> change_draw_intersecting;
};

struct GlobalData {
  std::vector<DebugInstance> instances;
  std::unordered_map<uint32_t, Vec3f> tag_colors;
} global_data;

DebugInstance* find_instance(AccelInstanceHandle accel) {
  for (auto& inst : global_data.instances) {
    if (inst.accel == accel) {
      return &inst;
    }
  }
  return nullptr;
}

OBB3f make_initial_bounds() {
  OBB3f result{};
  result.i = ConstVec3f::positive_x;
  result.j = ConstVec3f::positive_y;
  result.k = ConstVec3f::positive_z;
  result.half_size = Vec3f{1.0f};
  return result;
}

DebugInstance make_instance(
  AccelInstanceHandle accel, transform::TransformInstance* tform,
  editor::TransformEditorHandle tform_editor) {
  //
  DebugInstance result{};
  result.accel = accel;
  result.accessor_id = AccessorID::create();
  result.intersect_bounds = make_initial_bounds();
  result.transform = tform;
  result.transform_editor = tform_editor;
//  result.draw_intersecting = true;
  return result;
}

std::vector<const bounds::Element*>
find_intersecting(const Accel* accel, const DebugInstance& inst) {
  std::vector<const bounds::Element*> result;
  accel->intersects(bounds::make_query_element(inst.intersect_bounds), result);
  return result;
}

void draw_intersecting(const std::vector<const bounds::Element*>& hit,
                       const DebugInstance& inst,
                       const DebugBoundsSystemUpdateInfo&) {
  vk::debug::draw_obb3(inst.intersect_bounds, Vec3f{1.0f, 0.0f, 0.0f});
  for (const auto* el : hit) {
    auto& bounds = el->bounds;
    if (global_data.tag_colors.count(el->tag) == 0) {
      global_data.tag_colors[el->tag] = Vec3f{urandf(), urandf(), urandf()};
    }
    Vec3f color = global_data.tag_colors.at(el->tag);
    vk::debug::draw_obb3(bounds, color);
  }
}

void update_transform(DebugInstance& inst) {
  auto& curr = inst.transform->get_current();
  Vec3f pos = curr.translation;
  Vec3f scale = curr.scale;
  inst.intersect_bounds.position = pos;
  inst.intersect_bounds.half_size = scale * 0.5f;
}

void update_instance(DebugInstance& inst, const DebugBoundsSystemUpdateInfo& info) {
  update_transform(inst);

  if (inst.change_draw_intersecting) {
    inst.draw_intersecting = inst.change_draw_intersecting.value();
    inst.change_draw_intersecting = NullOpt{};
  }

  if (inst.draw_intersecting) {
    auto* accel = bounds::request_read(info.bounds_system, inst.accel, inst.accessor_id);
    if (accel) {
      draw_intersecting(find_intersecting(accel, inst), inst, info);
      bounds::release_read(info.bounds_system, inst.accel, inst.accessor_id);
    }

    info.editor->transform_editor.set_disabled(inst.transform_editor, false);
  } else {
    info.editor->transform_editor.set_disabled(inst.transform_editor, true);
  }
}

} //  anon

void bounds::debug::create_debug_accel_instance(
  AccelInstanceHandle accel, transform::TransformInstance* tform,
  editor::TransformEditorHandle tform_editor) {
  global_data.instances.emplace_back() = make_instance(accel, tform, tform_editor);
}

void bounds::debug::update_debug_bounds_system(const DebugBoundsSystemUpdateInfo& info) {
  for (auto& inst : global_data.instances) {
    update_instance(inst, info);
  }
}

Vec3f bounds::debug::get_intersection_drawing_bounds_scale(AccelInstanceHandle accel) {
  if (auto* inst = find_instance(accel)) {
    return inst->transform->get_current().scale;
  } else {
    return Vec3f{};
  }
}

void bounds::debug::set_intersection_drawing_bounds_scale(AccelInstanceHandle accel,
                                                          const Vec3f& scl) {
  if (auto* inst = find_instance(accel)) {
    auto trs = inst->transform->get_current();
    trs.scale = scl;
    inst->transform->set(trs);
  }
}

void bounds::debug::set_draw_intersections(AccelInstanceHandle accel, bool v) {
  if (auto* inst = find_instance(accel)) {
    inst->change_draw_intersecting = v;
  }
}

bool bounds::debug::intersection_drawing_enabled(AccelInstanceHandle accel) {
  if (auto* inst = find_instance(accel)) {
    return inst->draw_intersecting;
  } else {
    return false;
  }
}

GROVE_NAMESPACE_END
