#include "distribute_foliage_outwards_from_nodes.hpp"
#include "grove/math/random.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/util.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

uint32_t foliage::distribute_foliage_outwards_from_nodes(const FoliageDistributionParams& params,
                                                         FoliageDistributionEntry* dst_entries) {
  const int num_steps = params.num_steps;
  const int num_instances_per_step = params.num_instances_per_step;
  assert(num_steps > 0 && num_instances_per_step > 0);

  const float min_x = params.translation_log_min_x;
  const float max_x = params.translation_log_max_x;

  const float spread_scale = params.translation_step_spread_scale;
  const float step_pow = params.translation_step_power;
  const float rand_z_rot = params.rand_z_rotation_scale;

  const float len_scale = params.translation_x_scale;
  const float desc_scale = params.translation_y_scale;
  const float base_desc = std::log(max_x);

  const auto dir = params.outwards_direction;
  auto frame_x2 = normalize(exclude(dir, 1) * 2.0f - 1.0f);
  const Vec3f frame_x{frame_x2.x, 0.0f, frame_x2.y};
  const Vec3f frame_z{-frame_x.z, 0.0f, frame_x.x};

  uint32_t dst_ind{};
  for (int i = 0; i < num_steps; i++) {
    const float x_frac = std::pow(float(i) / float(num_steps - 1), step_pow);
    float desc = std::log(lerp(1.0f - x_frac, min_x, max_x)) - base_desc;
    auto len_off = frame_x * x_frac * len_scale;
    auto desc_off = Vec3f{0.0f, desc * desc_scale, 0.0f};
    auto inst_trans = params.tip_position + len_off + desc_off;

    const Vec3f up_dir = i == 0 ?
      frame_x : normalize(inst_trans - dst_entries[dst_ind - 1].translation);

    if (i == 1) {
      for (int j = 0; j < num_instances_per_step; j++) {
        auto& entry = dst_entries[j];
        entry.right_dir = frame_z;
        entry.forwards_dir = up_dir;
      }
    }

    inst_trans += frame_z * lerp(urandf(), -spread_scale, spread_scale);
//    inst_trans += frame_z * lerp(0.5f, -spread_scale, spread_scale);

    float inst_rand = urandf();
    float base_z_rot = lerp(urandf(), -pif() * rand_z_rot, pif() * 0.125f);
//    float base_z_rot = lerp(0.5f, -pif() * rand_z_rot, pif() * 0.125f);
    for (int j = 0; j < num_instances_per_step; j++) {
      float zrot = j == 0 ? 0.0f : j == 1 ? -pif() * 0.25f : pif() * 0.25f;
      float yrot = j == 0 ? 0.0f : j == 1 ? -pif() * 0.25f : pif() * 0.25f;
      zrot += base_z_rot;

      FoliageDistributionEntry entry{};
      entry.right_dir = frame_z;
      entry.forwards_dir = up_dir;
      entry.translation = inst_trans;
      entry.y_rotation = yrot;
      entry.z_rotation = zrot;
      entry.randomness = inst_rand;
      dst_entries[dst_ind++] = entry;
    }
  }

  return dst_ind;
}

GROVE_NAMESPACE_END
