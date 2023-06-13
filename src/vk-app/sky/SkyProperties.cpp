#include "SkyProperties.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

template <typename T>
auto property_ptrs(T&& self) {
  DynamicArray<decltype(&self.gradient_mid_points), 32> props;
  props.push_back(&self.clamp_z);
  props.push_back(&self.color_texture_index);
  props.push_back(&self.gradient_mid_points);
  props.push_back(&self.y0_color);
  props.push_back(&self.y1_color);
  props.push_back(&self.y2_color);
  props.push_back(&self.y3_color);
  props.push_back(&self.draw_sun);
  props.push_back(&self.sun_position);
  props.push_back(&self.sun_color);
  props.push_back(&self.sun_scale);
  props.push_back(&self.sun_offset);
  props.push_back(&self.manual_sky_color_control);
  props.push_back(&self.manual_sun_color_control);
  props.push_back(&self.manual_sun_position_control);
  props.push_back(&self.spherical_sun_position_control);
  props.push_back(&self.sun_position_incr);
  props.push_back(&self.increase_sun_position_theta);
  return props;
}

} //  anon

EditorPropertySet SkyProperties::property_set() const {
  EditorPropertySet result{self};

  auto prop_ptrs = property_ptrs(*this);
  for (auto& prop : prop_ptrs) {
    result.properties.push_back(*prop);
  }

  return result;
}

SkyProperties::ToCommit SkyProperties::update(const EditorPropertyChangeView& changes) {
  auto own_changes = changes.view_by_parent(self);
  DynamicArray<EditorPropertyHistoryItem, 2> to_commit;

  auto props = property_ptrs(*this);
  for (auto& prop : props) {
    auto prop_changes = own_changes.view_by_self(prop->descriptor.ids.self);
    maybe_update_property_data(prop_changes, prop, to_commit);
  }

  return to_commit;
}

void SkyProperties::copy_from_params(const SkyGradient::Params& params) {
  gradient_mid_points.data = EditorPropertyData{Vec3f{params.y1, params.y2, 0.0f}};
  y0_color.data = EditorPropertyData{params.y0_color};
  y1_color.data = EditorPropertyData{params.y1_color};
  y2_color.data = EditorPropertyData{params.y2_color};
  y3_color.data = EditorPropertyData{params.y3_color};
}

GROVE_NAMESPACE_END
