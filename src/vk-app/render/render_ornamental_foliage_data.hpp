#pragma once

#include "render_ornamental_foliage_descriptors.hpp"
#include "render_ornamental_foliage_types.hpp"
#include "grove/common/ContiguousElementGroupAllocator.hpp"

namespace grove::foliage {

struct OrnamentalFoliageInstanceHandle {
  bool is_valid() const {
    return flags != 0;
  }
  bool is_small_data() const;
  bool is_large_data() const;

  uint16_t page;
  uint16_t group;
  uint8_t flags;
  uint32_t aggregate_index_one_based;
};

struct OrnamentalFoliageData {
public:
  static constexpr uint32_t instance_page_size = 512; //  @ most N instances per page

  struct Page {
    ContiguousElementGroupAllocator group_alloc;
    uint32_t offset{};
    uint32_t size{};
    bool modified{};
  };

  struct InstanceMeta {
    OrnamentalFoliageGeometryType geometry_type;
    OrnamentalFoliageMaterialType material_type;
  };

  template <typename T>
  struct InstanceSet {
    void clear_modified() {
      pages_modified = false;
      for (auto& page : pages) {
        page.modified = false;
      }
    }

    uint32_t num_instances() const {
      return uint32_t(instances.size());
    }

    uint32_t num_pages() const {
      return uint32_t(pages.size());
    }

    std::vector<T> instances;
    std::vector<InstanceMeta> instance_meta;
    std::vector<Page> pages;
    bool pages_modified{};
  };

public:
  void clear_modified() {
    small_instances.clear_modified();
    large_instances.clear_modified();
    large_instance_aggregate_data_modified = false;
  }

public:
  InstanceSet<OrnamentalFoliageSmallInstanceData> small_instances;
  InstanceSet<OrnamentalFoliageLargeInstanceData> large_instances;
  std::vector<OrnamentalFoliageLargeInstanceAggregateData> large_instance_aggregate_data;
  std::vector<uint32_t> free_large_instance_aggregates;
  bool large_instance_aggregate_data_modified{};

  uint32_t max_material1_texture_layer_index{};
  uint32_t max_material2_texture_layer_index{};
};

OrnamentalFoliageInstanceHandle create_ornamental_foliage_instances(
  OrnamentalFoliageData* data,
  const OrnamentalFoliageInstanceGroupDescriptor& group_desc,
  const OrnamentalFoliageInstanceDescriptor* descriptors, uint32_t num_instances);

void destroy_ornamental_foliage_instances(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle);

void set_ornamental_foliage_curved_plane_geometry(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle,
  const CurvedPlaneGeometryDescriptor& geom);

void set_ornamental_foliage_flat_plane_scale(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float scale);
void set_ornamental_foliage_curved_plane_radius(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float r);

void set_ornamental_foliage_material2_colors(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle,
  const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
  const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3);

void set_global_ornamental_foliage_material2_colors(
  OrnamentalFoliageData* data,
  const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
  const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3);

OrnamentalFoliageData* get_global_ornamental_foliage_data();

}