#include "render_ornamental_foliage_data.hpp"
#include "grove/common/common.hpp"
#include "grove/common/pack.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace foliage;

struct Config {
  static constexpr uint8_t flag_small_data = uint8_t(1);
  static constexpr uint8_t flag_large_data = uint8_t(2);
};

template <typename Element>
using InstanceSet = OrnamentalFoliageData::InstanceSet<Element>;
using InstanceMeta = OrnamentalFoliageData::InstanceMeta;

uint32_t pack_3u8_1u32(const Vec3<uint8_t>& c) {
  return pack::pack_4u8_1u32(c.x, c.y, c.z, 0);
}

template <typename Instance>
void set_colors(Instance& inst,
                const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
                const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3) {
  inst.color0 = pack_3u8_1u32(c0);
  inst.color1 = pack_3u8_1u32(c1);
  inst.color2 = pack_3u8_1u32(c2);
  inst.color3 = pack_3u8_1u32(c3);
}

template <typename Instance>
void set_material1_colors(Instance& inst, const OrnamentalFoliageMaterial1Descriptor& desc) {
  inst.color0 = pack_3u8_1u32(desc.color0);
  inst.color1 = pack_3u8_1u32(desc.color1);
  inst.color2 = pack_3u8_1u32(desc.color2);
  inst.color3 = pack_3u8_1u32(desc.color3);
}

template <typename Instance>
void set_curved_plane_geometry(Instance& inst, const CurvedPlaneGeometryDescriptor& desc) {
  inst.min_radius = desc.min_radius;
  inst.radius = desc.radius;
  inst.radius_power = desc.radius_power;
  inst.curl_scale = desc.curl_scale;
}

template <typename Instance>
void set_branch_axis_wind_info(
  Instance& inst, const OrnamentalFoliageWindDataDescriptor::OnBranchAxis& desc) {
  //
  inst.wind_info0 = desc.info0;
  inst.wind_info1 = desc.info1;
  inst.wind_info2 = desc.info2;
}

template <typename Instance>
void set_flat_plane_scale(Instance& inst, float scale) {
  inst.scale = scale;
}

template <typename Instance>
void set_curved_plane_radius(Instance& inst, float r) {
  inst.radius = r;
}

OrnamentalFoliageSmallInstanceData
to_curved_plane_geometry_material1_plant_stem_wind_type(const OrnamentalFoliageInstanceDescriptor& desc) {
  OrnamentalFoliageSmallInstanceData result{};
  result.translation_direction_x = Vec4f{desc.translation, desc.orientation.x};
  result.direction_yz_unused = Vec4f{desc.orientation.y, desc.orientation.z, 0.0f, 0.0f};
  set_curved_plane_geometry(result, desc.geometry_descriptor.curved_plane);
  result.tip_y_fraction = desc.wind_data.on_plant_stem.tip_y_fraction;
  result.world_origin_x = desc.wind_data.on_plant_stem.world_origin_xz.x;
  result.world_origin_z = desc.wind_data.on_plant_stem.world_origin_xz.y;
  result.texture_layer_index = desc.material.material1.texture_layer_index;
  set_material1_colors(result, desc.material.material1);
  return result;
}

OrnamentalFoliageLargeInstanceData
to_curved_plane_geometry_material1_branch_axis_wind_type(
  const OrnamentalFoliageInstanceDescriptor& desc, uint32_t aggregate_index) {
  //
  OrnamentalFoliageLargeInstanceData result{};
  result.translation_direction_x = Vec4f{desc.translation, desc.orientation.x};
  result.direction_yz_unused = Vec4f{desc.orientation.y, desc.orientation.z, 0.0f, 0.0f};
  set_curved_plane_geometry(result, desc.geometry_descriptor.curved_plane);
  set_branch_axis_wind_info(result, desc.wind_data.on_branch_axis);
  set_material1_colors(result, desc.material.material1);
  result.texture_layer_index = desc.material.material1.texture_layer_index;
  result.aggregate_index = aggregate_index;
  return result;
}

OrnamentalFoliageSmallInstanceData
to_flat_plane_geometry_material2_plant_stem_wind_type(const OrnamentalFoliageInstanceDescriptor& desc) {
  OrnamentalFoliageSmallInstanceData result{};
  result.translation_direction_x = Vec4f{desc.translation, desc.orientation.x};
  result.direction_yz_unused = Vec4f{desc.orientation.y, desc.orientation.z, 0.0f, 0.0f};
  result.aspect = desc.geometry_descriptor.flat_plane.aspect;
  result.radius = desc.geometry_descriptor.flat_plane.scale;
  result.y_rotation_theta = desc.geometry_descriptor.flat_plane.y_rotation_theta;
  result.tip_y_fraction = desc.wind_data.on_plant_stem.tip_y_fraction;
  result.world_origin_x = desc.wind_data.on_plant_stem.world_origin_xz.x;
  result.world_origin_z = desc.wind_data.on_plant_stem.world_origin_xz.y;
  result.texture_layer_index = desc.material.material2.texture_layer_index;
  result.color0 = pack_3u8_1u32(desc.material.material2.color0);
  result.color1 = pack_3u8_1u32(desc.material.material2.color1);
  result.color2 = pack_3u8_1u32(desc.material.material2.color2);
  result.color3 = pack_3u8_1u32(desc.material.material2.color3);
  return result;
}

OrnamentalFoliageLargeInstanceData
to_flat_plane_geometry_material2_branch_axis_wind_type(
  const OrnamentalFoliageInstanceDescriptor& desc, uint32_t aggregate_index) {
  //
  OrnamentalFoliageLargeInstanceData result{};
  result.translation_direction_x = Vec4f{desc.translation, desc.orientation.x};
  result.direction_yz_unused = Vec4f{desc.orientation.y, desc.orientation.z, 0.0f, 0.0f};
  result.aspect = desc.geometry_descriptor.flat_plane.aspect;
  result.scale = desc.geometry_descriptor.flat_plane.scale;
  result.y_rotation_theta = desc.geometry_descriptor.flat_plane.y_rotation_theta;
  result.texture_layer_index = desc.material.material2.texture_layer_index;
  result.color0 = pack_3u8_1u32(desc.material.material2.color0);
  result.color1 = pack_3u8_1u32(desc.material.material2.color1);
  result.color2 = pack_3u8_1u32(desc.material.material2.color2);
  result.color3 = pack_3u8_1u32(desc.material.material2.color3);
  result.aggregate_index = aggregate_index;
  set_branch_axis_wind_info(result, desc.wind_data.on_branch_axis);
  return result;
}

InstanceMeta to_instance_meta(const OrnamentalFoliageInstanceGroupDescriptor& group_desc) {
  InstanceMeta result{};
  result.material_type = group_desc.material_type;
  result.geometry_type = group_desc.geometry_type;
  return result;
}

OrnamentalFoliageLargeInstanceAggregateData
to_large_instance_aggregate_data(const OrnamentalFoliageInstanceGroupDescriptor& group_desc) {
  OrnamentalFoliageLargeInstanceAggregateData result{};
  result.aggregate_aabb_p0 = Vec4f{group_desc.aggregate_aabb_p0, 0.0f};
  result.aggregate_aabb_p1 = Vec4f{group_desc.aggregate_aabb_p1, 0.0f};
  return result;
}

[[maybe_unused]]
void validate_small_instances(const OrnamentalFoliageInstanceGroupDescriptor& desc) {
  assert(desc.wind_type == OrnamentalFoliageWindType::OnPlantStem);

  if (desc.geometry_type == OrnamentalFoliageGeometryType::CurvedPlane) {
    assert(desc.material_type == OrnamentalFoliageMaterialType::Material1);

  } else if (desc.geometry_type == OrnamentalFoliageGeometryType::FlatPlane) {
    assert(desc.material_type == OrnamentalFoliageMaterialType::Material2);

  } else {
    assert(false);
  }

  (void) desc;
}

[[maybe_unused]]
void validate_large_instances(const OrnamentalFoliageInstanceGroupDescriptor& desc) {
  assert(desc.wind_type == OrnamentalFoliageWindType::OnBranchAxis);
  if (desc.geometry_type == foliage::OrnamentalFoliageGeometryType::CurvedPlane) {
    assert(desc.material_type == OrnamentalFoliageMaterialType::Material1);
  } else {
    assert(desc.geometry_type == foliage::OrnamentalFoliageGeometryType::FlatPlane);
    assert(desc.material_type == OrnamentalFoliageMaterialType::Material2);
  }
  (void) desc;
}

template <typename T>
OrnamentalFoliageInstanceHandle reserve(T& data_set, uint32_t num_instances, uint32_t* dst_offset) {
  assert(data_set.instances.size() == data_set.instance_meta.size());
  constexpr uint32_t page_size = OrnamentalFoliageData::instance_page_size;
  assert(num_instances <= page_size);

  OrnamentalFoliageData::Page* dst_page{};
  for (auto& page : data_set.pages) {
    if (page.size + num_instances <= page_size) {
      dst_page = &page;
      break;
    }
  }

  if (!dst_page) {
    dst_page = &data_set.pages.emplace_back();
    dst_page->offset = data_set.num_instances();
    data_set.instances.resize(data_set.instances.size() + page_size);
    data_set.instance_meta.resize(data_set.instance_meta.size() + page_size);
  }

  ContiguousElementGroupAllocator::ElementGroupHandle gh{};
  *dst_offset = dst_page->size + dst_page->offset;
  const uint32_t dst_size = dst_page->group_alloc.reserve(num_instances, &gh);
  assert(dst_size == dst_page->size + num_instances);
  (void) dst_size;
  dst_page->size += num_instances;

  data_set.pages_modified = true;
  dst_page->modified = true;

  OrnamentalFoliageInstanceHandle result{};
  result.page = uint16_t(dst_page - data_set.pages.data());
  result.group = uint16_t(gh.index);
  assert(data_set.instances.size() == data_set.instance_meta.size());
  return result;
}

template <typename Element>
void release(InstanceSet<Element>& data_set, OrnamentalFoliageInstanceHandle handle) {
  assert(data_set.instances.size() == data_set.instance_meta.size());

  const uint16_t page_ind = handle.page;
  assert(page_ind < data_set.num_pages());
  auto& page = data_set.pages[page_ind];

  const auto gh = ContiguousElementGroupAllocator::ElementGroupHandle{handle.group};
  (void) page.group_alloc.release(gh);
  ContiguousElementGroupAllocator::Movement move{};
  (void) page.group_alloc.arrange_implicit(&move, &page.size);

  Element* sd = data_set.instances.data() + page.offset;
  move.apply(sd, sizeof(Element));

  InstanceMeta* im = data_set.instance_meta.data() + page.offset;
  move.apply(im, sizeof(InstanceMeta));

  data_set.pages_modified = true;
  page.modified = true;
}

template <typename T>
void get_instances(
  InstanceSet<T>& data_set, OrnamentalFoliageInstanceHandle handle,
  const ContiguousElementGroupAllocator::ElementGroup** group, OrnamentalFoliageData::Page** page) {
  //
  assert(handle.page < data_set.num_pages());
  *page = &data_set.pages[handle.page];
  *group = data_set.pages[handle.page].group_alloc.read_group(
    ContiguousElementGroupAllocator::ElementGroupHandle{handle.group});
}

void set_max_texture_layer_indices(
  OrnamentalFoliageData* data, const OrnamentalFoliageInstanceGroupDescriptor& group_desc,
  const OrnamentalFoliageInstanceDescriptor* descriptors, uint32_t num_instances) {
  //
  if (group_desc.material_type == foliage::OrnamentalFoliageMaterialType::Material1) {
    for (uint32_t i = 0; i < num_instances; i++) {
      data->max_material1_texture_layer_index = std::max(
        data->max_material1_texture_layer_index,
        descriptors[i].material.material1.texture_layer_index);
    }
  } else if (group_desc.material_type == foliage::OrnamentalFoliageMaterialType::Material2) {
    for (uint32_t i = 0; i < num_instances; i++) {
      data->max_material2_texture_layer_index = std::max(
        data->max_material2_texture_layer_index,
        descriptors[i].material.material2.texture_layer_index);
    }
  } else {
    assert(false);
  }
}

OrnamentalFoliageInstanceHandle create_small_instances(
  OrnamentalFoliageData* data,
  const OrnamentalFoliageInstanceGroupDescriptor& group_desc,
  const OrnamentalFoliageInstanceDescriptor* descriptors, uint32_t num_instances) {
  //
#ifdef GROVE_DEBUG
  validate_small_instances(group_desc);
#endif
  set_max_texture_layer_indices(data, group_desc, descriptors, num_instances);

  auto& data_set = data->small_instances;

  uint32_t dst_off{};
  OrnamentalFoliageInstanceHandle result = reserve(data_set, num_instances, &dst_off);
  result.flags |= Config::flag_small_data;

  for (uint32_t i = 0; i < num_instances; i++) {
    const auto& src = descriptors[i];

    assert(dst_off + i < data_set.num_instances());
    auto& dst_inst = data_set.instances[dst_off + i];
    auto& dst_meta = data_set.instance_meta[dst_off + i];

    if (group_desc.geometry_type == OrnamentalFoliageGeometryType::CurvedPlane) {
      dst_inst = to_curved_plane_geometry_material1_plant_stem_wind_type(src);
    } else {
      dst_inst = to_flat_plane_geometry_material2_plant_stem_wind_type(src);
    }

    dst_meta = to_instance_meta(group_desc);
  }

  return result;
}

OrnamentalFoliageInstanceHandle create_large_instances(
  OrnamentalFoliageData* data,
  const OrnamentalFoliageInstanceGroupDescriptor& group_desc,
  const OrnamentalFoliageInstanceDescriptor* descriptors, uint32_t num_instances) {
  //
#ifdef GROVE_DEBUG
  validate_large_instances(group_desc);
#endif
  set_max_texture_layer_indices(data, group_desc, descriptors, num_instances);

  auto& data_set = data->large_instances;

  uint32_t dst_off{};
  OrnamentalFoliageInstanceHandle result = reserve(data_set, num_instances, &dst_off);
  result.flags |= Config::flag_large_data;

  uint32_t aggregate_index;
  if (!data->free_large_instance_aggregates.empty()) {
    aggregate_index = data->free_large_instance_aggregates.back();
    data->free_large_instance_aggregates.pop_back();
  } else {
    aggregate_index = uint32_t(data->large_instance_aggregate_data.size());
  }

  result.aggregate_index_one_based = aggregate_index + 1;

  for (uint32_t i = 0; i < num_instances; i++) {
    const auto& src = descriptors[i];
    assert(dst_off + i < data_set.num_instances());
    auto& dst_inst = data_set.instances[dst_off + i];
    auto& dst_meta = data_set.instance_meta[dst_off + i];

    if (group_desc.geometry_type == OrnamentalFoliageGeometryType::FlatPlane) {
      dst_inst = to_flat_plane_geometry_material2_branch_axis_wind_type(src, aggregate_index);
    } else {
      dst_inst = to_curved_plane_geometry_material1_branch_axis_wind_type(src, aggregate_index);
    }

    dst_meta = to_instance_meta(group_desc);
  }

  while (aggregate_index >= uint32_t(data->large_instance_aggregate_data.size())) {
    data->large_instance_aggregate_data.emplace_back();
  }

  data->large_instance_aggregate_data[aggregate_index] = to_large_instance_aggregate_data(group_desc);
  data->large_instance_aggregate_data_modified = true;
  return result;
}

template <typename T>
void set_flat_plane_scale(
  InstanceSet<T>& data_set, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float scale) {
  //
  OrnamentalFoliageData::Page* page;
  const ContiguousElementGroupAllocator::ElementGroup* group;
  get_instances(data_set, handle, &group, &page);

  assert(offset < group->count);
  const uint32_t dst_off = offset + group->offset + page->offset;
  assert(dst_off < data_set.num_instances());
  assert(data_set.instance_meta[dst_off].geometry_type == OrnamentalFoliageGeometryType::FlatPlane);
  set_flat_plane_scale(data_set.instances[dst_off], scale);

  data_set.pages_modified = true;
  page->modified = true;
}

template <typename T>
void set_curved_plane_radius(
  InstanceSet<T>& data_set, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float r) {
  //
  OrnamentalFoliageData::Page* page;
  const ContiguousElementGroupAllocator::ElementGroup* group;
  get_instances(data_set, handle, &group, &page);

  assert(offset < group->count);
  const uint32_t dst_off = offset + group->offset + page->offset;
  assert(dst_off < data_set.num_instances());
  assert(data_set.instance_meta[dst_off].geometry_type == OrnamentalFoliageGeometryType::CurvedPlane);
  set_curved_plane_radius(data_set.instances[dst_off], r);

  data_set.pages_modified = true;
  page->modified = true;
}

template <typename T>
void set_global_material2_colors(
  InstanceSet<T>& data_set, const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
  const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3) {
  //
  for (auto& page : data_set.pages) {
    for (uint32_t i = page.offset; i < page.offset + page.size; i++) {
      if (data_set.instance_meta[i].material_type == OrnamentalFoliageMaterialType::Material2) {
        set_colors(data_set.instances[i], c0, c1, c2, c3);
        data_set.pages_modified = true;
        page.modified = true;
      }
    }
  }
}

struct {
  OrnamentalFoliageData data;
} globals;

} //  anon

OrnamentalFoliageInstanceHandle
foliage::create_ornamental_foliage_instances(
  OrnamentalFoliageData* data,
  const OrnamentalFoliageInstanceGroupDescriptor& group_desc,
  const OrnamentalFoliageInstanceDescriptor* descriptors, uint32_t num_instances) {
  //
  const bool small_instances = group_desc.wind_type == OrnamentalFoliageWindType::OnPlantStem;
  if (small_instances) {
    return create_small_instances(data, group_desc, descriptors, num_instances);
  } else {
    return create_large_instances(data, group_desc, descriptors, num_instances);
  }
}

void foliage::set_global_ornamental_foliage_material2_colors(
  OrnamentalFoliageData* data,
  const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
  const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3) {
  //
  set_global_material2_colors(data->small_instances, c0, c1, c2, c3);
  set_global_material2_colors(data->large_instances, c0, c1, c2, c3);
}

void foliage::set_ornamental_foliage_material2_colors(
  OrnamentalFoliageData* data,
  OrnamentalFoliageInstanceHandle handle,
  const Vec3<uint8_t>& c0, const Vec3<uint8_t>& c1,
  const Vec3<uint8_t>& c2, const Vec3<uint8_t>& c3) {
  //
  assert(handle.is_small_data());

  auto& data_set = data->small_instances;
  OrnamentalFoliageData::Page* page;
  const ContiguousElementGroupAllocator::ElementGroup* group;
  get_instances(data_set, handle, &group, &page);

  for (uint32_t i = 0; i < group->count; i++) {
    const uint32_t dst_off = i + group->offset + page->offset;
    assert(dst_off < data_set.num_instances());
    assert(data_set.instance_meta[dst_off].material_type == OrnamentalFoliageMaterialType::Material2);
    set_colors(data_set.instances[dst_off], c0, c1, c2, c3);
  }

  data_set.pages_modified = true;
  page->modified = true;
}

void foliage::set_ornamental_foliage_curved_plane_geometry(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle,
  const CurvedPlaneGeometryDescriptor& geom) {
  //
  assert(handle.is_small_data());

  auto& data_set = data->small_instances;
  OrnamentalFoliageData::Page* page;
  const ContiguousElementGroupAllocator::ElementGroup* group;
  get_instances(data_set, handle, &group, &page);

  for (uint32_t i = 0; i < group->count; i++) {
    const uint32_t dst_off = i + group->offset + page->offset;
    assert(dst_off < data_set.num_instances());
    assert(data_set.instance_meta[dst_off].geometry_type == OrnamentalFoliageGeometryType::CurvedPlane);
    set_curved_plane_geometry(data_set.instances[dst_off], geom);
  }

  data_set.pages_modified = true;
  page->modified = true;
}

void foliage::set_ornamental_foliage_flat_plane_scale(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float scale) {
  //
  if (handle.is_small_data()) {
    set_flat_plane_scale(data->small_instances, handle, offset, scale);
  } else {
    assert(handle.is_large_data());
    set_flat_plane_scale(data->large_instances, handle, offset, scale);
  }
}

void foliage::set_ornamental_foliage_curved_plane_radius(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle, uint32_t offset, float r) {
  //
  if (handle.is_small_data()) {
    set_curved_plane_radius(data->small_instances, handle, offset, r);
  } else {
    assert(handle.is_large_data());
    set_curved_plane_radius(data->large_instances, handle, offset, r);
  }
}

void foliage::destroy_ornamental_foliage_instances(
  OrnamentalFoliageData* data, OrnamentalFoliageInstanceHandle handle) {
  //
  if (handle.flags & Config::flag_small_data) {
    release(data->small_instances, handle);

  } else {
    assert(handle.is_large_data());
    release(data->large_instances, handle);

    assert(handle.aggregate_index_one_based != 0);
    const uint32_t return_agg = handle.aggregate_index_one_based - 1;
#ifdef GROVE_DEBUG
    auto it = std::find(
      data->free_large_instance_aggregates.begin(),
      data->free_large_instance_aggregates.end(), return_agg);
    assert(it == data->free_large_instance_aggregates.end());
#endif
    data->free_large_instance_aggregates.push_back(return_agg);
  }
}

OrnamentalFoliageData* foliage::get_global_ornamental_foliage_data() {
  return &globals.data;
}

bool foliage::OrnamentalFoliageInstanceHandle::is_small_data() const {
  return flags & Config::flag_small_data;
}

bool foliage::OrnamentalFoliageInstanceHandle::is_large_data() const {
  return flags & Config::flag_large_data;
}

GROVE_NAMESPACE_END
