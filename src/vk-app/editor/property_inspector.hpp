#pragma once

namespace grove {
  struct EditorProperty;
  struct EditorPropertyData;
  struct EditorPropertyDescriptor;

  template <typename T>
  class Optional;

  template <typename T>
  struct Mat4;

  template <typename T>
  struct Vec3;
}

namespace grove::properties {

Optional<EditorPropertyData> imgui_render_editor_property(const EditorProperty& prop);
Optional<EditorPropertyData>
imgui_render_mat4_property_data(const EditorPropertyDescriptor& descriptor, const grove::Mat4<float>& m);

Optional<EditorPropertyData> imgui_render_vec3_editor_property_slider(const EditorProperty& prop,
                                                                      const Vec3<float>& min,
                                                                      const Vec3<float>& max);
Optional<EditorPropertyData> imgui_render_vec3_editor_property_slider01(const EditorProperty& prop);
}