#include "audio_signal.hpp"
#include "../procedural_tree/components.hpp"
#include "../procedural_tree/render.hpp"
#include "grove/math/constants.hpp"
#include "grove/math/util.hpp"
#include "grove/common/Temporary.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

bool fit(const tree::Internode* nodes, int num_nodes, float* dst, int num_dst) {
  auto bounds = tree::internode_aabb(nodes, uint32_t(num_nodes));
  if (prod(bounds.size()) == 0) {
    return false;
  }

  std::fill(dst, dst + num_dst, 0.0f);

  auto cent = bounds.center();
  for (int i = 0; i < num_nodes; i++) {
    auto p = clamp_each(nodes[i].position, bounds.min, bounds.max);
    auto to_cent = p - cent;
    float v = to_cent.x;
    if (std::abs(to_cent.z) > std::abs(to_cent.x)) {
      v = to_cent.z;
    }

    auto p01 = (p - bounds.min) / (bounds.max - bounds.min);
    const int bin = clamp(int(p01.x * float(num_dst)), 0, num_dst - 1);
    dst[bin] += nodes[i].lateral_q * v;
  }

  { //  smooth
    Temporary<float, 2048> store_tmp;
    auto* tmp = store_tmp.require(num_dst);
    assert(!store_tmp.heap && "alloc required");

    const int win = 11;
    for (int i = 0; i < num_dst; i++) {
      float s{};
      float w{};
      for (int j = 0; j < win; j++) {
        int o = j - win / 2 + i;
        if (o >= 0 && o < num_dst) {
          s += dst[o];
          w += 1.0f;
        }
      }
      tmp[i] = s / w;
    }
    memcpy(dst, tmp, num_dst * sizeof(float));
  }

  int num_zero_bins{};
  float max_len{-1.0f};
  for (int i = 0; i < num_dst; i++) {
    float t = std::abs(dst[i]);
    if (t == 0) {
      num_zero_bins++;
    }
    if (t > max_len) {
      max_len = t;
    }
  }

  if (float(num_zero_bins) / float(num_dst) > 0.75f) {
    //  arbitrary - reject if many bins are unfilled.
    return false;

  } else if (max_len <= 0) {
    return false;
  }

  const double period_over_sz = grove::two_pi() / double(num_dst);
  for (int i = 0; i < num_dst; i++) {
    float v = clamp((dst[i] / max_len) * 2.0f - 1.0f, -1.0f, 1.0f);
    dst[i] = lerp(0.25f, float(std::sin(double(i) * period_over_sz)), v);
  }

  return true;
}

} //  anon

void tree::make_wave_from_internodes(
  const tree::Internode* nodes, int num_nodes, float* dst, int num_dst) {
  //
  if (num_dst == 0) {
    return;
  }

  bool fill_with_sin{};
  if (num_nodes > 0) {
    fill_with_sin = !fit(nodes, num_nodes, dst, num_dst);
  } else {
    fill_with_sin = true;
  }

  if (fill_with_sin) {
    const double period_over_sz = grove::two_pi() / double(num_dst);
    for (int i = 0; i < num_dst; i++) {
      dst[i] = float(std::sin(double(i) * period_over_sz));
    }
  }
}

GROVE_NAMESPACE_END
