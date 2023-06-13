#pragma once

#include "bounds_system.hpp"

namespace grove::transform {
class TransformInstance;
}

namespace grove::editor {
struct TransformEditorHandle;
struct Editor;
}

namespace grove::bounds::debug {

struct DebugBoundsSystemUpdateInfo {
  editor::Editor* editor;
  bounds::BoundsSystem* bounds_system;
};

void create_debug_accel_instance(
  AccelInstanceHandle accel,
  transform::TransformInstance* tform, editor::TransformEditorHandle tform_editor);
void update_debug_bounds_system(const DebugBoundsSystemUpdateInfo& info);

void set_draw_intersections(AccelInstanceHandle accel, bool v);
void set_intersection_drawing_bounds_scale(AccelInstanceHandle accel, const Vec3f& scl);
Vec3f get_intersection_drawing_bounds_scale(AccelInstanceHandle accel);
bool intersection_drawing_enabled(AccelInstanceHandle accel);

}