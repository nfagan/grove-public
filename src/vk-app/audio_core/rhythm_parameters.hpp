#pragma once

namespace grove {

struct RhythmParameters {
  void set_global_p_quantized(float p);

  float global_p_quantized{0.8f};
};

}