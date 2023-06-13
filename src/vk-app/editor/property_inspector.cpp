#include "property_inspector.hpp"
#include "properties.hpp"
#include "grove/common/common.hpp"
#include "grove/math/matrix.hpp"
#include <imgui/imgui.h>

GROVE_NAMESPACE_BEGIN

namespace {

std::string make_entity_id_tagged_label(const char* label, const Entity& entity) {
  std::string result{label};
  result += "##";
  result += std::to_string(entity.id);
  return result;
}

Optional<EditorPropertyData> render_float_property(const EditorProperty& prop) {
  auto maybe_data = prop.data.read_float();
  assert(maybe_data);

  if (!maybe_data) {
    return NullOpt{};
  }

  float res{maybe_data.value()};
  const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  auto label =
    make_entity_id_tagged_label(prop.descriptor.label, prop.descriptor.ids.self);

  if (ImGui::InputFloat(label.c_str(), &res, 0, 0, "%0.2f", enter_flag)) {
    return Optional<EditorPropertyData>(EditorPropertyData{res});
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData> render_int_property(const EditorProperty& prop) {
  auto maybe_data = prop.data.read_int();
  assert(maybe_data);

  if (!maybe_data) {
    return NullOpt{};
  }

  int res{maybe_data.value()};
  const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  auto label =
    make_entity_id_tagged_label(prop.descriptor.label, prop.descriptor.ids.self);

  if (ImGui::InputInt(label.c_str(), &res, 0, 0, enter_flag)) {
    return Optional<EditorPropertyData>(EditorPropertyData{res});
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData> render_bool_property(const EditorProperty& prop) {
  auto maybe_data = prop.data.read_bool();
  assert(maybe_data);

  if (!maybe_data) {
    return NullOpt{};
  }

  bool res{maybe_data.value()};
  auto label =
    make_entity_id_tagged_label(prop.descriptor.label, prop.descriptor.ids.self);

  if (ImGui::Checkbox(label.c_str(), &res)) {
    return Optional<EditorPropertyData>(EditorPropertyData{res});
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData> render_vec3_property(const EditorProperty& prop) {
  auto maybe_data = prop.data.read_vec3();
  assert(maybe_data);

  if (!maybe_data) {
    return NullOpt{};
  }

  auto [x, y, z] = maybe_data.value();
  float res[3] = {x, y, z};
  auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
  auto label =
    make_entity_id_tagged_label(prop.descriptor.label, prop.descriptor.ids.self);

  if (ImGui::InputFloat3(label.c_str(), &res[0], "%0.2f", enter_flag)) {
    Vec3f update{res[0], res[1], res[2]};
    return Optional<EditorPropertyData>(EditorPropertyData{update});
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData> render_custom_property(const EditorProperty& prop) {
  auto maybe_data = prop.data.read_custom();
  assert(maybe_data);

  if (!maybe_data) {
    return NullOpt{};
  }

  auto& custom = maybe_data.value();
  return custom->gui_render(prop.descriptor);
}

} //  anon

Optional<EditorPropertyData> properties::imgui_render_editor_property(const EditorProperty& prop) {
  switch (prop.data.type) {
    case EditorPropertyData::Type::Float:
      return render_float_property(prop);

    case EditorPropertyData::Type::Int:
      return render_int_property(prop);

    case EditorPropertyData::Type::Bool:
      return render_bool_property(prop);

    case EditorPropertyData::Type::Vec3:
      return render_vec3_property(prop);

    case EditorPropertyData::Type::Custom:
      return render_custom_property(prop);

    default:
      assert(false);
  }

  return NullOpt{};
}

Optional<EditorPropertyData>
properties::imgui_render_mat4_property_data(const EditorPropertyDescriptor& descriptor,
                                            const grove::Mat4<float>& m) {
  auto maybe_modify = m;
  bool modified = false;

  for (int i = 0; i < 4; i++) {
    auto t = maybe_modify[i];
    auto [x, y, z, w] = t;
    float res[4] = {x, y, z, w};
    auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;
    auto label =
      make_entity_id_tagged_label(descriptor.label, descriptor.ids.self);
    label += std::to_string(i);

    if (ImGui::InputFloat4(label.c_str(), &res[0], "%0.2f", enter_flag)) {
      maybe_modify[i] = Vec4f{res[0], res[1], res[2], res[3]};
      modified = true;
    }
  }

  if (modified) {
    return Optional<EditorPropertyData>(make_mat4_editor_property_data(maybe_modify));
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData>
properties::imgui_render_vec3_editor_property_slider(const EditorProperty& prop,
                                                     const Vec3<float>& min,
                                                     const Vec3<float>& max) {
  Vec3f prop_data = prop.read_or_default(Vec3f{});
  bool modified{};
  for (int i = 0; i < 3; i++) {
    std::string component_label{prop.descriptor.label};
    component_label += std::to_string(i);
    if (ImGui::SliderFloat(component_label.c_str(), &prop_data[i], min[i], max[i])) {
      modified = true;
    }
  }

  if (modified) {
    return Optional<EditorPropertyData>(EditorPropertyData{prop_data});
  } else {
    return NullOpt{};
  }
}

Optional<EditorPropertyData>
properties::imgui_render_vec3_editor_property_slider01(const EditorProperty& prop) {
  return imgui_render_vec3_editor_property_slider(prop, Vec3f{}, Vec3f{1.0f});
}

GROVE_NAMESPACE_END
