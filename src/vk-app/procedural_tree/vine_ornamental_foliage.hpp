#pragma once

namespace grove::foliage {
struct OrnamentalFoliageData;
}

namespace grove::tree {

struct VineSystem;
struct TreeSystem;
struct VineInstanceHandle;
struct VineSegmentHandle;

struct VineOrnamentalFoliageUpdateInfo {
  const VineSystem* vine_sys;
  const TreeSystem* tree_sys;
  foliage::OrnamentalFoliageData* render_data;
};

struct VineOrnamentalFoliageUpdateResult {
  int num_finished_growing;
};

void create_ornamental_foliage_on_vine_segment(
  const VineInstanceHandle& inst, const VineSegmentHandle& seg);

VineOrnamentalFoliageUpdateResult
update_ornamental_foliage_on_vines(const VineOrnamentalFoliageUpdateInfo& info);

}