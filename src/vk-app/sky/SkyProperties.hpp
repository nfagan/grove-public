#pragma once

#include "../editor/properties.hpp"
#include "SkyGradient.hpp"

namespace grove {

struct SkyProperties {
  using ToCommit = DynamicArray<EditorPropertyHistoryItem, 2>;

  static constexpr Vec3f blue = Vec3f(75.0f/255.0f, 143.0f/255.0f, 233.0f/255.0f);
  static constexpr Vec3f white = Vec3f(249.0f/255.0f, 250.0f/255.0f, 241.0f/255.0f);

public:
  EditorPropertySet property_set() const;
  ToCommit update(const EditorPropertyChangeView& changes);
  void copy_from_params(const SkyGradient::Params& params);

public:
  Entity self{Entity::create()};
  EditorProperty clamp_z{GROVE_MAKE_NEW_EDITOR_PROPERTY("clamp_z", self, true)};
  EditorProperty color_texture_index{GROVE_MAKE_NEW_EDITOR_PROPERTY("color_texture_index", self, 2)};
  EditorProperty gradient_mid_points{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("gradient_mid_points", self, Vec3f(0.47f, 0.64f, 0.0f))
  };
  EditorProperty y0_color{GROVE_MAKE_NEW_EDITOR_PROPERTY("y0_color", self, white)};
  EditorProperty y1_color{GROVE_MAKE_NEW_EDITOR_PROPERTY("y1_color", self, white)};
  EditorProperty y2_color{GROVE_MAKE_NEW_EDITOR_PROPERTY("y2_color", self, blue)};
  EditorProperty y3_color{GROVE_MAKE_NEW_EDITOR_PROPERTY("y3_color", self, Vec3f(0.1f))};
  EditorProperty draw_sun{GROVE_MAKE_NEW_EDITOR_PROPERTY("draw_sun", self, false)};
  EditorProperty sun_position{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("sun_position", self, Vec3f(10.0f, 50.0f, 100.0f))};
  EditorProperty sun_color{GROVE_MAKE_NEW_EDITOR_PROPERTY("sun_color", self, Vec3f(1.0f))};
  EditorProperty sun_scale{GROVE_MAKE_NEW_EDITOR_PROPERTY("sun_scale", self, Vec3f{128.0f})};
  EditorProperty sun_offset{GROVE_MAKE_NEW_EDITOR_PROPERTY("sun_offset", self, 2048.0f)};
  EditorProperty manual_sky_color_control{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("manual_sky_color_control", self, false)
  };
  EditorProperty manual_sun_color_control{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("manual_sun_color_control", self, false)
  };
  EditorProperty spherical_sun_position_control{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("spherical_sun_position_control", self, false)
  };
  EditorProperty manual_sun_position_control{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("manual_sun_position_control", self, true)
  };
  EditorProperty sun_position_incr{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("sun_position_incr", self, 0.0001f)
  };
  EditorProperty increase_sun_position_theta{
    GROVE_MAKE_NEW_EDITOR_PROPERTY("increase_sun_position_theta", self, false)
  };
};

}