#pragma once

#include "cursor.hpp"
#include "../transform/transform_system.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/Mat4.hpp"
#include "grove/common/Optional.hpp"
#include <array>
#include <unordered_set>

namespace grove::editor {

struct TransformEditorHandle {
  GROVE_INTEGER_IDENTIFIER_STD_HASH(Hash, TransformEditorHandle, id)
  GROVE_INTEGER_IDENTIFIER_EQUALITY(TransformEditorHandle, id)
  GROVE_INTEGER_IDENTIFIER_IS_VALID(id)
  uint32_t id;
};

class TransformEditor {
public:
  struct ToChange {
    TransformEditorHandle target;
    Optional<Vec3f> translation;
    Optional<float> y_rotation;
    Optional<float> x_rotation;
  };

  struct InstanceDrawableInfo {
    Vec3f scale;
    Vec3f color;
    bool disabled;
  };

  struct Instance {
    transform::TransformInstance* transform;
    std::array<cursor::Monitorable*, 4> cursor_monitorables;
    std::array<InstanceDrawableInfo, 4> drawable_info;
    uint32_t num_selectables;
    Vec3f last_cursor_position;
    Optional<uint32_t> active_plane_index;
    bool first_hit;
  };

  struct UpdateInfo {
    const Ray& cursor_ray;
    bool cursor_down;
    cursor::Monitor& cursor_monitor;
  };

  using Instances = std::unordered_map<TransformEditorHandle,
                                       Instance,
                                       TransformEditorHandle::Hash>;
public:
  void update(const UpdateInfo& info);
  TransformEditorHandle create_instance(transform::TransformInstance* target,
                                        transform::TransformSystem& transform_system,
                                        cursor::Monitor& cursor_monitor,
                                        const Vec3f& position);
  void destroy_instance(TransformEditorHandle, cursor::Monitor& cursor_monitor);
  void set_color(TransformEditorHandle handle, const Vec3f& color);
  void set_disabled(TransformEditorHandle handle, bool disable);
  const Instances& read_instances() const {
    return instances;
  }
  bool has_active_instance() const {
    return !active_instances.empty();
  }
  int num_instances() const {
    return int(instances.size());
  }

private:
  uint32_t next_transform_editor_id{1};
  Instances instances;
  std::unordered_set<TransformEditorHandle, TransformEditorHandle::Hash> active_instances;
};

}