#pragma once

#include "types.hpp"

namespace grove {
  struct TextureParameters;
}

struct grove::TextureParameters {
  TextureFilterMethod min_filter;
  TextureFilterMethod mag_filter;
  
  TextureWrapMethod wrap_s;
  TextureWrapMethod wrap_t;
  TextureWrapMethod wrap_r;
  
  static TextureParameters edge_clamp_linear() {
    TextureParameters params{};
    
    params.wrap_s = TextureWrapMethod::EdgeClamp;
    params.wrap_t = TextureWrapMethod::EdgeClamp;
    params.wrap_r = TextureWrapMethod::EdgeClamp;
    params.min_filter = TextureFilterMethod::Linear;
    params.mag_filter = TextureFilterMethod::Linear;
    
    return params;
  }

  static TextureParameters repeat_linear() {
    TextureParameters params{};

    params.wrap_s = TextureWrapMethod::Repeat;
    params.wrap_t = TextureWrapMethod::Repeat;
    params.wrap_r = TextureWrapMethod::Repeat;
    params.min_filter = TextureFilterMethod::Linear;
    params.mag_filter = TextureFilterMethod::Linear;

    return params;
  }
};