#pragma once

#include "grove/math/vector.hpp"

namespace grove::gui {

struct QuadVertex {
  Vec4f xy_unused;
  Vec4f instance_centroid_and_dimensions;
  Vec4f instance_radius_fraction_and_border_size_and_opacity;
  Vec4<uint32_t> instance_color_and_border_color;
};

struct GlyphQuadVertex {
  Vec4f position_uv;
  Vec4<uint32_t> texture_layer_color_unused;
};

}