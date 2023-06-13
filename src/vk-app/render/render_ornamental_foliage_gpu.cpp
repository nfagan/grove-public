#include "render_ornamental_foliage_gpu.hpp"
#include "render_ornamental_foliage_data.hpp"
#include "graphics.hpp"
#include "debug_label.hpp"
#include "../procedural_flower/geometry.hpp"
#include "csm.hpp"
#include "../util/texture_io.hpp"
#include "shadow.hpp"
#include "DynamicSampledImageManager.hpp"
#include "SampledImageManager.hpp"
#include "grove/env.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/visual/Image.hpp"
#include "grove/load/image.hpp"
#include "grove/common/common.hpp"
#include <numeric>

GROVE_NAMESPACE_BEGIN

namespace {

struct Config {
  static constexpr int num_material1_alpha_texture_layers = 8;
};

using BeginFrameInfo = foliage::RenderOrnamentalFoliageBeginFrameInfo;
using RenderForwardInfo = foliage::RenderOrnamentalFoliageRenderForwardInfo;

Optional<std::vector<grove::Image<uint8_t>>>
load_images(const std::string& im_dir, const char** im_names, int num_images, int expect_components) {
  std::vector<grove::Image<uint8_t>> images;
  for (int i = 0; i < num_images; i++) {
    auto im_p = im_dir + im_names[i];
    bool success{};
    auto im = load_image(im_p.c_str(), &success, true);
    if (!success || im.num_components_per_pixel != expect_components) {
      return NullOpt{};
    }
    images.emplace_back(std::move(im));
  }
  return Optional<std::vector<grove::Image<uint8_t>>>(std::move(images));
}

auto create_alpha_test_material_image(vk::SampledImageManager& im_manager) {
  struct Result {
    int num_layers;
    Optional<vk::SampledImageManager::Handle> image;
  };

  Result result{};

  std::string res_dir{GROVE_ASSET_DIR};
  std::vector<std::string> mat_ims;
#if 1
  mat_ims.emplace_back("/textures/ornament/petal1_material-lily2.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-lily-dots2.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-clematis-dots.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-rose1.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-rose2.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-daisy-orig.png");
  mat_ims.emplace_back("/textures/ornament/petal1_material-daisy.png");
  assert(int(mat_ims.size() == Config::num_material1_alpha_texture_layers));
#else
  mat_ims.emplace_back("/test/ornament_texture/petal1_material-rose1.png");
  mat_ims.emplace_back("/test/ornament_texture/petal1_material-daffodil-center-dots.png");
#endif

  std::vector<Image<uint8_t>> images;
  for (auto& im_file : mat_ims) {
    auto im_p = res_dir + im_file;
    bool success;
    auto im = load_image(im_p.c_str(), &success, true);
    if (!success) {
      return result;
    } else {
      images.emplace_back(std::move(im));
    }
  }

  auto data = pack_texture_layers(images);
  if (!data) {
    return result;
  }

  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
  create_info.data = data.get();
  create_info.descriptor = image::Descriptor::make_2d_uint8n(
    images[0].width, images[0].height, images[0].num_components_per_pixel);
  create_info.descriptor.shape.depth = int(images.size());

  result.image = im_manager.create_sync(create_info);
  result.num_layers = int(images.size());
  return result;
}

Optional<vk::SampledImageManager::Handle>
create_flat_plane_color_array_image(const BeginFrameInfo& info) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/";
  im_dir += "experiment/";

  const char* im_names[5] = {
    "tiled1-small.png",
    "tiled2-small.png",
    "tiled3-small.png",
    "tiled4-small.png",
    "tiled5-small.png",
  };

  auto images = load_images(im_dir, im_names, 5, 4);
  if (!images) {
    return NullOpt{};
  }
  auto res = pack_texture_layers<uint8_t>(images.value());
  if (!res) {
    return NullOpt{};
  }

  auto& ims = images.value();
  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(ims[0].width, ims[0].height, int(ims.size())),
    image::Channels::make_uint8n(4)
  };
  create_info.data = res.get();
  create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager->create_sync(create_info);
}

Optional<vk::SampledImageManager::Handle>
create_flat_plane_alpha_test_array_image(const BeginFrameInfo& info) {
  auto im_dir = std::string{GROVE_ASSET_DIR} + "/textures/";

#if 0
  const char* im_names[5] = {
    "tree-leaves-tiny/maple-leaf-revisit.png",
    "ornamental-foliage-tiny/vine.png",
    "tree-leaves-tiny/elm-leaf.png",
    "tree-leaves-tiny/broad-leaf1-no-border.png",
    "tree-leaves-tiny/thin-leaves1.png"
  };
#else
  const char* im_names[5] = {
    "tree-leaves/maple-leaf-revisit.png",
    "ornamental-foliage/vine.png",
    "tree-leaves/elm-leaf.png",
    "tree-leaves/broad-leaf1-no-border.png",
    "tree-leaves/thin-leaves1.png"
  };
#endif

  Optional<std::vector<grove::Image<uint8_t>>> images = load_images(im_dir, im_names, 5, 4);
  if (!images) {
    return NullOpt{};
  }

  auto res = pack_texture_layers(images.value());
  if (!res) {
    return NullOpt{};
  }

  auto& ims = images.value();
  vk::SampledImageManager::ImageCreateInfo create_info{};
  create_info.descriptor = {
    image::Shape::make_3d(ims[0].width, ims[0].height, int(ims.size())),
    image::Channels::make_uint8n(4)
  };
  create_info.data = res.get();
  create_info.int_conversion = IntConversion::UNorm;
  create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  create_info.image_type = vk::SampledImageManager::ImageType::Image2DArray;
  create_info.sample_in_stages = {vk::PipelineStage::FragmentShader};
  return info.sampled_image_manager->create_sync(create_info);
}

} //  anon

using namespace vk;
using namespace foliage;

template <typename Element>
using InstanceSet = OrnamentalFoliageData::InstanceSet<Element>;

struct StemWindPushConstantData {
  Mat4f projection_view;
  Vec4f num_grid_points_xz_t_unused;
  Vec4f wind_world_bound_xz;
};

struct BranchWindPushConstantData {
  Mat4f projection_view;
  Vec4f num_grid_points_xz_t_unused;
  Vec4f wind_world_bound_xz;
  Vec4f wind_displacement_info;
};

struct UniformBufferData {
  csm::SunCSMSampleData csm_sample_data;
  Mat4f view;
  Mat4f sun_light_view_projection0;
  Vec4f camera_position;
  Vec4f sun_color;
};

struct GridGeometryBuffer {
  gfx::BufferHandle geom;
  gfx::BufferHandle index;
  uint32_t num_indices{};
  GridGeometryParams geometry_params{};
  bool valid{};
};

struct DynamicArrayBuffer {
  gfx::BufferHandle buffer;
  uint32_t num_reserved{};
  uint32_t num_active{};
  bool modified{};
  std::vector<bool> pages_modified;
};

struct DynamicArrayBuffers {
  void require(uint32_t frame_queue_depth) {
    buffers.resize(frame_queue_depth);
  }

  DynamicArray<DynamicArrayBuffer, 3> buffers;
  bool valid{};
};

struct VertexInstanceIndexBuffer {
  gfx::BufferHandle buff;
  uint32_t num_active{};
  uint32_t num_reserved{};
  bool valid{};
  std::bitset<32> modified{};
};

struct FoliagePipeline {
  gfx::PipelineHandle pipeline_handle;
  Optional<VkDescriptorSet> desc_set0;
};

struct GPUContext {
  VertexInstanceIndexBuffer curved_plane_small_instance_indices;
  VertexInstanceIndexBuffer curved_plane_large_instance_indices;
  VertexInstanceIndexBuffer flat_plane_small_instance_indices;
  VertexInstanceIndexBuffer flat_plane_large_instance_indices;

  DynamicArrayBuffers small_instance_buffers;
  DynamicArrayBuffers large_instance_buffers;
  DynamicArrayBuffer large_instance_aggregate_buffer;

  FoliagePipeline curved_plane_geometry_stem_wind_pipeline;
  FoliagePipeline curved_plane_geometry_branch_wind_pipeline;
  FoliagePipeline flat_plane_geometry_stem_wind_pipeline;
  FoliagePipeline flat_plane_geometry_branch_wind_pipeline;

  GridGeometryBuffer lod0_curved_plane_geometry_buffer;
  gfx::DynamicUniformBuffer global_uniform_buffer;
  RenderOrnamentalFoliageRenderParams render_params{};

  Optional<DynamicSampledImageManager::Handle> wind_image;
  Optional<SampledImageManager::Handle> material1_image;
  Optional<SampledImageManager::Handle> material2_alpha_image;
  Optional<SampledImageManager::Handle> material2_color_image;

  std::vector<uint32_t> tmp_indices;

  bool tried_initialize{};
  bool disabled{};
  bool wrote_to_instance_buffers{};
  bool wrote_to_indices_buffers{};
};

namespace {

GridGeometryParams lod0_curved_plane_grid_geometry_params() {
  GridGeometryParams result{};
  result.num_pts_x = 9;
  result.num_pts_z = 11;
  return result;
}

Optional<GridGeometryBuffer> create_grid_geometry_buffer(const BeginFrameInfo& info,
                                                         const GridGeometryParams& geom_params) {
  GridGeometryBuffer result{};
  result.geometry_params = geom_params;

  auto geom = make_reflected_grid_indices(geom_params);
  auto inds = triangulate_reflected_grid(geom_params);

  auto geom_buff = create_device_local_vertex_buffer_sync(
    info.graphics_context, geom.size() * sizeof(float), geom.data());
  if (!geom_buff) {
    return NullOpt{};
  } else {
    result.geom = std::move(geom_buff.value());
  }

  auto ind_buff = create_device_local_index_buffer_sync(
    info.graphics_context, inds.size() * sizeof(uint16_t), inds.data());
  if (!ind_buff) {
    return NullOpt{};
  } else {
    result.index = std::move(ind_buff.value());
    result.num_indices = uint32_t(inds.size());
  }

  result.valid = true;
  return Optional<GridGeometryBuffer>(std::move(result));
}

auto create_global_uniform_buffer(const BeginFrameInfo& info) {
  return create_dynamic_uniform_buffer<UniformBufferData>(
    info.graphics_context, info.frame_queue_depth);
}

glsl::PreprocessorDefinitions shadow_preprocessor_defs() {
  glsl::PreprocessorDefinitions result;
  result.push_back(csm::make_num_sun_shadow_cascades_preprocessor_definition());
  result.push_back(csm::make_default_num_sun_shadow_samples_preprocessor_definition());
  return result;
}

VkDescriptorType reflect_branch_wind_desc_type(const glsl::refl::DescriptorInfo& info) {
  if (info.is_uniform_buffer()) {
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  } else if (info.is_storage_buffer() && info.binding == 1) {
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
  } else {
    return refl::identity_descriptor_type(info);
  }
}

Optional<glsl::VertFragProgramSource> create_curved_plane_stem_wind_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "orn-foliage/curved-plane-stem-wind.vert";
  params.frag_file = "orn-foliage/curved-plane-stem-wind.frag";
  params.compile.vert_defines = shadow_preprocessor_defs();
  params.compile.frag_defines = shadow_preprocessor_defs();
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_curved_plane_branch_wind_program_source() {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "orn-foliage/curved-plane-branch-wind.vert";
  params.frag_file = "orn-foliage/curved-plane-stem-wind.frag";
  params.compile.vert_defines = shadow_preprocessor_defs();
  params.compile.frag_defines = shadow_preprocessor_defs();
  params.compile.frag_defines.push_back(glsl::make_define("IS_BRANCH_WIND"));
  params.reflect.to_vk_descriptor_type = reflect_branch_wind_desc_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_flat_plane_stem_wind_program_source(bool use_alpha_to_cov) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "orn-foliage/flat-plane-stem-wind.vert";
  params.frag_file = "orn-foliage/flat-plane-stem-wind.frag";
  params.compile.vert_defines = shadow_preprocessor_defs();
  params.compile.frag_defines = shadow_preprocessor_defs();
  if (use_alpha_to_cov) {
    params.compile.frag_defines.push_back(glsl::make_define("ENABLE_ALPHA_TO_COV"));
  }
  params.reflect.to_vk_descriptor_type = vk::refl::always_dynamic_uniform_buffer_descriptor_type;
  return glsl::make_vert_frag_program_source(params);
}

Optional<glsl::VertFragProgramSource> create_flat_plane_branch_wind_program_source(bool use_alpha_to_cov) {
  glsl::LoadVertFragProgramSourceParams params{};
  params.vert_file = "orn-foliage/flat-plane-branch-wind.vert";
  params.frag_file = "orn-foliage/flat-plane-stem-wind.frag";
  params.compile.vert_defines = shadow_preprocessor_defs();
  params.compile.frag_defines = shadow_preprocessor_defs();
  params.compile.frag_defines.push_back(glsl::make_define("IS_BRANCH_WIND"));
  if (use_alpha_to_cov) {
    params.compile.frag_defines.push_back(glsl::make_define("ENABLE_ALPHA_TO_COV"));
  }
  params.reflect.to_vk_descriptor_type = reflect_branch_wind_desc_type;

  return glsl::make_vert_frag_program_source(params);
}

template <typename GetSource>
Optional<gfx::PipelineHandle> create_pipeline(
  const BeginFrameInfo& info, const GetSource& get_source, bool enable_alpha_to_coverage) {

  auto source = get_source();
  if (!source) {
    return NullOpt{};
  }

  auto pass = gfx::get_forward_write_back_render_pass_handle(info.graphics_context);
  if (!pass) {
    return NullOpt{};
  }

  int loc{};
  VertexBufferDescriptor buff_descs[2];
  buff_descs[0].add_attribute(AttributeDescriptor::float2(loc++));
  buff_descs[1].add_attribute(AttributeDescriptor::unconverted_unsigned_intn(loc++, 1, 1));

  gfx::GraphicsPipelineCreateInfo create_info{};
  create_info.enable_alpha_to_coverage = enable_alpha_to_coverage;
  create_info.disable_cull_face = true;
  create_info.num_color_attachments = 1;
  create_info.vertex_buffer_descriptors = buff_descs;
  create_info.num_vertex_buffer_descriptors = 2;
  return gfx::create_pipeline(info.graphics_context, std::move(source.value()), create_info, pass.value());
}

bool reserve_storage_buffer(DynamicArrayBuffer& buff, gfx::Context* graphics_context,
                            uint32_t num_activate, size_t element_size, uint32_t fq_depth) {
  buff.num_active = 0;

  uint32_t num_reserve = buff.num_reserved;
  while (num_reserve < num_activate) {
    num_reserve = num_reserve == 0 ? 128 : num_reserve * 2;
  }

  if (num_reserve != buff.num_reserved) {
    auto storage_buff = create_storage_buffer(graphics_context, num_reserve * element_size * fq_depth);
    if (!storage_buff) {
      return false;
    } else {
      buff.buffer = std::move(storage_buff.value());
      buff.num_reserved = num_reserve;
    }
  }

  buff.num_active = num_activate;
  return true;
}

bool reserve_storage_buffer(DynamicArrayBuffer& buff, gfx::Context* graphics_context,
                            uint32_t curr_num_insts, size_t element_size, bool* realloced) {
  uint32_t num_reserve = buff.num_reserved;
  while (num_reserve < curr_num_insts) {
    num_reserve = num_reserve == 0 ? 128 : num_reserve * 2;
  }

  *realloced = false;
  if (num_reserve != buff.num_reserved) {
    auto storage_buff = create_storage_buffer(graphics_context, num_reserve * element_size);
    if (!storage_buff) {
      return false;
    } else {
      buff.buffer = std::move(storage_buff.value());
      buff.num_reserved = num_reserve;
      *realloced = true;
    }
  }

  buff.num_active = curr_num_insts;
  return true;
}

template <typename Element>
void set_modified(DynamicArrayBuffers& buffs, const InstanceSet<Element>& data_set,
                  uint32_t frame_queue_depth) {
  assert(uint32_t(buffs.buffers.size()) >= frame_queue_depth);
  (void) frame_queue_depth;

  if (!data_set.pages_modified) {
    return;
  }

  for (auto& buff : buffs.buffers) {
    buff.modified = true;
    buff.pages_modified.resize(data_set.num_pages());
    for (uint32_t i = 0; i < data_set.num_pages(); i++) {
      if (data_set.pages[i].modified) {
        buff.pages_modified[i] = true;
      }
    }
  }
}

template <typename Element>
bool require_instance_buffers(DynamicArrayBuffers& buffs, gfx::Context* graphics_context,
                              const InstanceSet<Element>& data_set, uint32_t frame_index) {
  auto& curr_buff = buffs.buffers[frame_index];
  if (!curr_buff.modified) {
    return false;
  }

  const size_t el_size = sizeof(Element);
  const uint32_t curr_num_insts = data_set.num_instances();
  bool realloced{};
  bool reserve_res = reserve_storage_buffer(
    curr_buff, graphics_context, curr_num_insts, el_size, &realloced);
  if (!reserve_res) {
    buffs.valid = false;
    return false;
  }

  if (realloced) {
    std::fill(curr_buff.pages_modified.begin(), curr_buff.pages_modified.end(), true);
  }

  for (uint32_t i = 0; i < uint32_t(curr_buff.pages_modified.size()); i++) {
    if (curr_buff.pages_modified[i]) {
      const uint32_t inst_off = data_set.pages[i].offset;
      const size_t byte_off = inst_off * el_size;
      const size_t byte_size = OrnamentalFoliageData::instance_page_size * el_size;
      curr_buff.buffer.write(data_set.instances.data() + inst_off, byte_size, byte_off);
      curr_buff.pages_modified[i] = false;
    }
  }

  curr_buff.modified = false;
  buffs.valid = true;
  return true;
}

void prepare_small_instance_buffers(
  GPUContext& context, DynamicArrayBuffers& buffs, const BeginFrameInfo& info) {
  //
  buffs.require(info.frame_queue_depth);
  auto& data_set = info.cpu_data->small_instances;
  set_modified(buffs, data_set, info.frame_queue_depth);
  if (require_instance_buffers(buffs, info.graphics_context, data_set, info.frame_index)) {
    context.wrote_to_instance_buffers = true;
  }
}

void prepare_large_instance_buffers(
  GPUContext& context, DynamicArrayBuffers& buffs, const BeginFrameInfo& info) {
  //
  buffs.require(info.frame_queue_depth);
  auto& data_set = info.cpu_data->large_instances;
  set_modified(buffs, data_set, info.frame_queue_depth);
  if (require_instance_buffers(buffs, info.graphics_context, data_set, info.frame_index)) {
    context.wrote_to_instance_buffers = true;
  }
}

void prepare_large_instance_aggregate_buffer(
  GPUContext& context, DynamicArrayBuffer& buff, const BeginFrameInfo& info) {
  //
  buff.pages_modified.resize(info.frame_queue_depth);
  if (info.cpu_data->large_instance_aggregate_data_modified) {
    std::fill(buff.pages_modified.begin(), buff.pages_modified.end(), true);
  }

  const uint32_t num_insts = uint32_t(info.cpu_data->large_instance_aggregate_data.size());
  const size_t el_size = sizeof(OrnamentalFoliageLargeInstanceAggregateData);

  if (buff.pages_modified[info.frame_index]) {
    bool success = reserve_storage_buffer(
      buff, info.graphics_context, num_insts, el_size, info.frame_queue_depth);
    if (success) {
      const size_t off = el_size * info.frame_index * buff.num_reserved;
      buff.buffer.write(
        info.cpu_data->large_instance_aggregate_data.data(), el_size * num_insts, off);
      buff.pages_modified[info.frame_index] = false;
      context.wrote_to_instance_buffers = true;
    }
  }
}

template <typename Element, typename F>
bool prepare_instance_indices(GPUContext& context,
                              VertexInstanceIndexBuffer& inds, const InstanceSet<Element>& data_set,
                              const F& match_instance, const BeginFrameInfo& info) {
  if (data_set.pages_modified) {
    for (uint32_t i = 0; i < info.frame_queue_depth; i++) {
      inds.modified[i] = true;
    }
  }

  if (!inds.modified[info.frame_index]) {
    return false;
  }

  uint32_t num_reserve = inds.num_reserved;
  while (num_reserve < data_set.num_instances()) {
    num_reserve = num_reserve == 0 ? 128 : num_reserve * 2;
  }

  if (num_reserve != inds.num_reserved) {
    auto buff = create_host_visible_vertex_buffer(
      info.graphics_context, num_reserve * sizeof(uint32_t) * info.frame_queue_depth);
    if (!buff) {
      inds.valid = false;
      return false;
    } else {
      inds.buff = std::move(buff.value());
      inds.num_reserved = num_reserve;
      inds.valid = true;
    }
  }

  uint32_t num_active{};

  auto& tmp = context.tmp_indices;
  tmp.resize(data_set.num_instances());

#if 1
  for (auto& page : data_set.pages) {
    for (uint32_t i = 0; i < page.size; i++) {
      uint32_t ind = i + page.offset;
      if (match_instance(data_set.instance_meta[ind])) {
        tmp[num_active++] = ind;
      }
    }
  }
#else
  for (uint32_t i = 0; i < data_set.num_instances(); i++) {
    if (match_instance(data_set.instance_meta[i])) {
      tmp[num_active++] = i;
    }
  }
#endif

  inds.num_active = num_active;

  const size_t byte_off = inds.num_reserved * sizeof(uint32_t) * info.frame_index;
  inds.buff.write(tmp.data(), inds.num_active * sizeof(uint32_t), byte_off);

  inds.modified[info.frame_index] = false;
  return true;
}

void prepare_curved_plane_geometry_small_instance_indices(GPUContext& context, const BeginFrameInfo& info) {
  const auto& data_set = info.cpu_data->small_instances;
  auto& inds = context.curved_plane_small_instance_indices;

  const auto match_instance = [](const OrnamentalFoliageData::InstanceMeta& meta) {
    return meta.geometry_type == OrnamentalFoliageGeometryType::CurvedPlane;
  };

  if (prepare_instance_indices(context, inds, data_set, match_instance, info)) {
    context.wrote_to_indices_buffers = true;
  }
}

void prepare_curved_plane_geometry_large_instance_indices(GPUContext& context, const BeginFrameInfo& info) {
  const auto& data_set = info.cpu_data->large_instances;
  auto& inds = context.curved_plane_large_instance_indices;

  const auto match_instance = [](const OrnamentalFoliageData::InstanceMeta& meta) {
    return meta.geometry_type == OrnamentalFoliageGeometryType::CurvedPlane;
  };

  if (prepare_instance_indices(context, inds, data_set, match_instance, info)) {
    context.wrote_to_indices_buffers = true;
  }
}

void prepare_flat_plane_geometry_small_instance_indices(GPUContext& context, const BeginFrameInfo& info) {
  const auto& data_set = info.cpu_data->small_instances;
  auto& inds = context.flat_plane_small_instance_indices;

  const auto match_instance = [](const OrnamentalFoliageData::InstanceMeta& meta) {
    return meta.geometry_type == OrnamentalFoliageGeometryType::FlatPlane;
  };

  if (prepare_instance_indices(context, inds, data_set, match_instance, info)) {
    context.wrote_to_indices_buffers = true;
  }
}

void prepare_flat_plane_geometry_large_instance_indices(GPUContext& context, const BeginFrameInfo& info) {
  const auto& data_set = info.cpu_data->large_instances;
  auto& inds = context.flat_plane_large_instance_indices;

  const auto match_instance = [](const OrnamentalFoliageData::InstanceMeta& meta) {
    return meta.geometry_type == OrnamentalFoliageGeometryType::FlatPlane;
  };

  if (prepare_instance_indices(context, inds, data_set, match_instance, info)) {
    context.wrote_to_indices_buffers = true;
  }
}

void prepare_global_uniform_buffer(const GPUContext& context, const gfx::DynamicUniformBuffer& buff,
                                   const BeginFrameInfo& info) {
  if (!buff.is_valid()) {
    return;
  }

  UniformBufferData data{};
  data.csm_sample_data = csm::make_sun_csm_sample_data(info.csm_desc);
  data.view = info.camera.get_view();
  data.sun_light_view_projection0 = info.csm_desc.light_shadow_sample_view;
  data.camera_position = Vec4f{info.camera.get_position(), 0.0f};
  data.sun_color = Vec4f{context.render_params.sun_color, 0.0f};

  buff.buffer.write(&data, sizeof(data), buff.element_stride * info.frame_index);
}

Optional<SampleImageView>
get_wind_image(const GPUContext& context, const BeginFrameInfo& info) {
  Optional<SampleImageView> wind_im;
  if (context.wind_image) {
    if (auto im = info.dynamic_sampled_image_manager->get(context.wind_image.value())) {
      if (im.value().is_2d() && im.value().vertex_shader_sample_ok()) {
        wind_im = im.value().to_sample_image_view();
      }
    }
  }
  return wind_im;
}

Optional<SampleImageView>
get_material1_image(const GPUContext& context, const BeginFrameInfo& info) {
  Optional<SampleImageView> material1_im;
  if (context.material1_image) {
    if (auto im = info.sampled_image_manager->get(context.material1_image.value())) {
      if (im.value().is_2d_array() && im.value().fragment_shader_sample_ok()) {
        material1_im = im.value().to_sample_image_view();
      }
    }
  }
  return material1_im;
}

Optional<SampleImageView>
get_material2_alpha_image(const GPUContext& context, const BeginFrameInfo& info) {
  Optional<SampleImageView> material2_alpha_image;
  if (context.material2_alpha_image) {
    if (auto im = info.sampled_image_manager->get(context.material2_alpha_image.value())) {
      if (im.value().is_2d_array() && im.value().fragment_shader_sample_ok()) {
        material2_alpha_image = im.value().to_sample_image_view();
      }
    }
  }
  return material2_alpha_image;
}

[[maybe_unused]]
Optional<SampleImageView>
get_material2_color_image(const GPUContext& context, const BeginFrameInfo& info) {
  Optional<SampleImageView> material2_color_image;
  if (context.material2_color_image) {
    if (auto im = info.sampled_image_manager->get(context.material2_color_image.value())) {
      if (im.value().is_2d_array() && im.value().fragment_shader_sample_ok()) {
        material2_color_image = im.value().to_sample_image_view();
      }
    }
  }
  return material2_color_image;
}

[[maybe_unused]]
Optional<int> num_texture_layers(const SampledImageManager& im_manager,
                                 const Optional<SampledImageManager::Handle>& im_handle) {
  Optional<int> num_layers;
  if (im_handle) {
    if (auto im = im_manager.get(im_handle.value())) {
      if (im.value().is_2d_array()) {
        num_layers = im.value().descriptor.shape.depth;
      }
    }
  }
  return num_layers;
}

void prepare_curved_plane_geometry_stem_wind_pipeline(GPUContext& context, const BeginFrameInfo& info) {
  auto& pipeline = context.curved_plane_geometry_stem_wind_pipeline;
  pipeline.desc_set0 = NullOpt{};

  if (!pipeline.pipeline_handle.is_valid()) {
    return;
  }

  auto& buffs = context.small_instance_buffers;
  if (!buffs.valid) {
    return;
  }

  auto& curr_buff = buffs.buffers[info.frame_index];
  auto& glob_un_buff = context.global_uniform_buffer;
  auto wind_im = get_wind_image(context, info);
  auto material1_im = get_material1_image(context, info);

  if (!material1_im || !wind_im || !glob_un_buff.is_valid() || curr_buff.num_active == 0) {
    return;
  }

  auto sampler_linear = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_storage_buffer(
    scaffold, bind++,
    curr_buff.buffer.get(), curr_buff.num_active * sizeof(OrnamentalFoliageSmallInstanceData));
  push_dynamic_uniform_buffer(
    scaffold, bind++, glob_un_buff.buffer.get(), sizeof(UniformBufferData));
  push_combined_image_sampler(
    scaffold, bind++, wind_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, material1_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_linear);

  pipeline.desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pipeline.pipeline_handle);
}

void prepare_curved_plane_geometry_branch_wind_pipeline(GPUContext& context, const BeginFrameInfo& info) {
  auto& pipeline = context.curved_plane_geometry_branch_wind_pipeline;
  pipeline.desc_set0 = NullOpt{};

  if (!pipeline.pipeline_handle.is_valid()) {
    return;
  }

  auto& buffs = context.large_instance_buffers;
  auto& agg_buff = context.large_instance_aggregate_buffer;
  if (!buffs.valid || agg_buff.num_active == 0) {
    return;
  }

  auto& curr_buff = buffs.buffers[info.frame_index];
  auto& glob_un_buff = context.global_uniform_buffer;
  auto wind_im = get_wind_image(context, info);
  auto material1_im = get_material1_image(context, info);

  if (!material1_im || !wind_im || !glob_un_buff.is_valid() || curr_buff.num_active == 0) {
    return;
  }

  auto sampler_linear = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_storage_buffer(
    scaffold, bind++,
    curr_buff.buffer.get(), curr_buff.num_active * sizeof(OrnamentalFoliageLargeInstanceData));
  push_dynamic_storage_buffer(
    scaffold, bind++, agg_buff.buffer.get(),
    agg_buff.num_active * sizeof(OrnamentalFoliageLargeInstanceAggregateData));
  push_dynamic_uniform_buffer(
    scaffold, bind++, glob_un_buff.buffer.get(), sizeof(UniformBufferData));
  push_combined_image_sampler(
    scaffold, bind++, wind_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, material1_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_linear);

  pipeline.desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pipeline.pipeline_handle);
}

void prepare_flat_plane_geometry_stem_wind_pipeline(GPUContext& context, const BeginFrameInfo& info) {
  auto& pipeline = context.flat_plane_geometry_stem_wind_pipeline;
  pipeline.desc_set0 = NullOpt{};

  if (!pipeline.pipeline_handle.is_valid()) {
    return;
  }

  auto& buffs = context.small_instance_buffers;
  if (!buffs.valid) {
    return;
  }

  auto& curr_buff = buffs.buffers[info.frame_index];
  auto& glob_un_buff = context.global_uniform_buffer;
  auto wind_im = get_wind_image(context, info);
  auto material2_alpha_im = get_material2_alpha_image(context, info);
//  auto material2_color_im = get_material2_color_image(context, info);

  if (!wind_im || !material2_alpha_im || !glob_un_buff.is_valid() || curr_buff.num_active == 0) {
    return;
  }

  auto sampler_linear = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);
  auto sampler_repeat = gfx::get_image_sampler_linear_repeat(info.graphics_context);
  (void) sampler_repeat;

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_storage_buffer(
    scaffold, bind++,
    curr_buff.buffer.get(), curr_buff.num_active * sizeof(OrnamentalFoliageSmallInstanceData));
  push_dynamic_uniform_buffer(
    scaffold, bind++, glob_un_buff.buffer.get(), sizeof(UniformBufferData));
  push_combined_image_sampler(
    scaffold, bind++, wind_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, material2_alpha_im.value(), sampler_linear);
//  push_combined_image_sampler(
//    scaffold, bind++, material2_color_im.value(), sampler_repeat);
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_linear);

  pipeline.desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pipeline.pipeline_handle);
}

void prepare_flat_plane_geometry_branch_wind_pipeline(GPUContext& context, const BeginFrameInfo& info) {
  auto& pipeline = context.flat_plane_geometry_branch_wind_pipeline;
  pipeline.desc_set0 = NullOpt{};

  if (!pipeline.pipeline_handle.is_valid()) {
    return;
  }

  auto& inst_buffs = context.large_instance_buffers;
  auto& agg_buff = context.large_instance_aggregate_buffer;
  if (!inst_buffs.valid || agg_buff.num_active == 0) {
    return;
  }

  auto& curr_buff = inst_buffs.buffers[info.frame_index];
  auto& glob_un_buff = context.global_uniform_buffer;
  auto wind_im = get_wind_image(context, info);
  auto material2_alpha_im = get_material2_alpha_image(context, info);
//  auto material2_color_im = get_material2_color_image(context, info);

  if (!wind_im || !material2_alpha_im || !glob_un_buff.is_valid() || curr_buff.num_active == 0) {
    return;
  }

  auto sampler_linear = gfx::get_image_sampler_linear_edge_clamp(info.graphics_context);
  auto sampler_repeat = gfx::get_image_sampler_linear_repeat(info.graphics_context);
  (void) sampler_repeat;

  vk::DescriptorSetScaffold scaffold;
  scaffold.set = 0;
  uint32_t bind{};
  push_storage_buffer(
    scaffold, bind++,
    curr_buff.buffer.get(), curr_buff.num_active * sizeof(OrnamentalFoliageLargeInstanceData));
  push_dynamic_storage_buffer(
    scaffold, bind++,
    agg_buff.buffer.get(), agg_buff.num_active * sizeof(OrnamentalFoliageLargeInstanceAggregateData));
  push_dynamic_uniform_buffer(
    scaffold, bind++, glob_un_buff.buffer.get(), sizeof(UniformBufferData));
  push_combined_image_sampler(
    scaffold, bind++, wind_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, material2_alpha_im.value(), sampler_linear);
  push_combined_image_sampler(
    scaffold, bind++, info.shadow_image, sampler_linear);

  pipeline.desc_set0 = gfx::require_updated_descriptor_set(
    info.graphics_context, scaffold, pipeline.pipeline_handle);
}

void lazy_init(GPUContext& context, const BeginFrameInfo& info) {
  {
    const bool use_alpha_to_cov = true;
    auto get_source = []() { return create_curved_plane_stem_wind_program_source(); };
    if (auto pd = create_pipeline(info, get_source, use_alpha_to_cov)) {
      context.curved_plane_geometry_stem_wind_pipeline.pipeline_handle = std::move(pd.value());
    } else {
      return;
    }
  }
  {
    const bool use_alpha_to_cov = true;
    auto get_source = []() { return create_curved_plane_branch_wind_program_source(); };
    if (auto pd = create_pipeline(info, get_source, use_alpha_to_cov)) {
      context.curved_plane_geometry_branch_wind_pipeline.pipeline_handle = std::move(pd.value());
    } else {
      return;
    }
  }
  {
    const bool use_alpha_to_cov = true;
    auto get_source = [use_alpha_to_cov]() {
      (void) use_alpha_to_cov;
      return create_flat_plane_stem_wind_program_source(use_alpha_to_cov);
    };
    if (auto pd = create_pipeline(info, get_source, use_alpha_to_cov)) {
      context.flat_plane_geometry_stem_wind_pipeline.pipeline_handle = std::move(pd.value());
    } else {
      return;
    }
  }
  {
    const bool use_alpha_to_cov = true;
    auto get_source = [use_alpha_to_cov]() {
      (void) use_alpha_to_cov;
      return create_flat_plane_branch_wind_program_source(use_alpha_to_cov);
    };
    if (auto pd = create_pipeline(info, get_source, use_alpha_to_cov)) {
      context.flat_plane_geometry_branch_wind_pipeline.pipeline_handle = std::move(pd.value());
    } else {
      return;
    }
  }

  if (auto buff = create_global_uniform_buffer(info)) {
    context.global_uniform_buffer = std::move(buff.value());
  }

  if (auto buff = create_grid_geometry_buffer(info, lod0_curved_plane_grid_geometry_params())) {
    context.lod0_curved_plane_geometry_buffer = std::move(buff.value());
  }

  if (auto im = create_flat_plane_color_array_image(info)) {
    context.material2_color_image = im.value();
  }

  {
    auto mat1_im = create_alpha_test_material_image(*info.sampled_image_manager);
    if (mat1_im.image) {
      context.material1_image = mat1_im.image.value();
    }
  }

  if (auto im = create_flat_plane_alpha_test_array_image(info)) {
    context.material2_alpha_image = im.value();
  }
}

void begin_frame(GPUContext* context, const BeginFrameInfo& info) {
  context->wrote_to_indices_buffers = false;
  context->wrote_to_instance_buffers = false;

  if (!context->tried_initialize) {
    lazy_init(*context, info);
    context->tried_initialize = true;
  }

#ifdef GROVE_DEBUG
  if (auto num_layers = num_texture_layers(*info.sampled_image_manager, context->material1_image)) {
    assert(int(info.cpu_data->max_material1_texture_layer_index) < num_layers.value());
  }
  if (auto num_layers = num_texture_layers(*info.sampled_image_manager, context->material2_alpha_image)) {
    assert(int(info.cpu_data->max_material2_texture_layer_index) < num_layers.value());
  }
#endif

  prepare_small_instance_buffers(*context, context->small_instance_buffers, info);
  prepare_large_instance_buffers(*context, context->large_instance_buffers, info);
  prepare_large_instance_aggregate_buffer(*context, context->large_instance_aggregate_buffer, info);
  prepare_curved_plane_geometry_small_instance_indices(*context, info);
  prepare_curved_plane_geometry_large_instance_indices(*context, info);
  prepare_flat_plane_geometry_small_instance_indices(*context, info);
  prepare_flat_plane_geometry_large_instance_indices(*context, info);
  prepare_global_uniform_buffer(*context, context->global_uniform_buffer, info);
  prepare_curved_plane_geometry_stem_wind_pipeline(*context, info);
  prepare_curved_plane_geometry_branch_wind_pipeline(*context, info);
  prepare_flat_plane_geometry_stem_wind_pipeline(*context, info);
  prepare_flat_plane_geometry_branch_wind_pipeline(*context, info);

  info.cpu_data->clear_modified();
}

BranchWindPushConstantData make_branch_wind_push_constant_data(
  const GPUContext& context, const GridGeometryParams& geom_params, const RenderForwardInfo& info) {
  //
  auto proj = info.camera.get_projection();
  proj[1] = -proj[1];
  BranchWindPushConstantData result{};
  result.wind_displacement_info = Vec4f{
    context.render_params.wind_displacement_limits.x,
    context.render_params.wind_displacement_limits.y,
    context.render_params.wind_strength_limits.x,
    context.render_params.wind_strength_limits.y
  };
  result.wind_world_bound_xz = context.render_params.wind_world_bound_xz;
  result.projection_view = proj * info.camera.get_view();
  result.num_grid_points_xz_t_unused = Vec4f{
    float(geom_params.num_pts_x),
    float(geom_params.num_pts_z),
    context.render_params.branch_elapsed_time, 0.0f
  };
  return result;
}

StemWindPushConstantData make_stem_wind_push_constant_data(
  const GPUContext& context, const GridGeometryParams& geom_params, const RenderForwardInfo& info) {
  //
  auto proj = info.camera.get_projection();
  proj[1] = -proj[1];
  StemWindPushConstantData result{};
  result.wind_world_bound_xz = context.render_params.wind_world_bound_xz;
  result.projection_view = proj * info.camera.get_view();
  result.num_grid_points_xz_t_unused = Vec4f{
    float(geom_params.num_pts_x),
    float(geom_params.num_pts_z),
    context.render_params.elapsed_time, 0.0f
  };
  return result;
}

template <typename PushConstants>
void draw_grid_geometry(
  GPUContext* context, const FoliagePipeline& pd, const GridGeometryBuffer& geom,
  const VertexInstanceIndexBuffer& inds, const PushConstants* pc_data, const RenderForwardInfo& info,
  const uint32_t* addtl_dyn_off = nullptr) {
  //
  uint32_t dyn_offs[2];
  uint32_t num_dyn_offs;
  if (addtl_dyn_off) {
    dyn_offs[0] = *addtl_dyn_off;
    dyn_offs[1] = uint32_t(context->global_uniform_buffer.element_stride) * info.frame_index;
    num_dyn_offs = 2;
  } else {
    dyn_offs[0] = uint32_t(context->global_uniform_buffer.element_stride) * info.frame_index;
    num_dyn_offs = 1;
  }

  cmd::bind_graphics_pipeline(info.cmd, pd.pipeline_handle.get());
  cmd::set_viewport_and_scissor(info.cmd, &info.viewport, &info.scissor);
  cmd::bind_graphics_descriptor_sets(
    info.cmd, pd.pipeline_handle.get_layout(), 0, 1, &pd.desc_set0.value(), num_dyn_offs, dyn_offs);
  cmd::push_constants(info.cmd, pd.pipeline_handle.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, pc_data);

  VkDeviceSize vb_offs[2] = {
    0,
    info.frame_index * sizeof(uint32_t) * inds.num_reserved
  };
  VkBuffer vert_buffs[2] = {
    geom.geom.get(),
    inds.buff.get()
  };

  vkCmdBindVertexBuffers(info.cmd, 0, 2, vert_buffs, vb_offs);
  auto ind_buff = geom.index.get();
  vkCmdBindIndexBuffer(info.cmd, ind_buff, 0, VK_INDEX_TYPE_UINT16);

  DrawIndexedDescriptor draw_desc{};
  draw_desc.num_instances = inds.num_active;
  draw_desc.num_indices = geom.num_indices;
  cmd::draw_indexed(info.cmd, &draw_desc);
}

void render_flat_plane_geometry_stem_wind_forward(GPUContext* context, const RenderForwardInfo& info) {
  auto& pd = context->flat_plane_geometry_stem_wind_pipeline;
  if (!pd.pipeline_handle.is_valid() || !pd.desc_set0) {
    return;
  }

  //  @TODO: Use a different plane with fewer triangles.
  auto& geom = context->lod0_curved_plane_geometry_buffer;
  auto& inds = context->flat_plane_small_instance_indices;
  if (inds.valid && geom.valid && inds.num_active > 0) {
    //  @TODO: Separate push constant type
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_flat_plane_geometry_stem_wind_forward");
    (void) profiler;

    auto pc_data = make_stem_wind_push_constant_data(*context, geom.geometry_params, info);
    draw_grid_geometry(context, pd, geom, inds, &pc_data, info);
  }
}

void render_flat_plane_geometry_branch_wind_forward(GPUContext* context, const RenderForwardInfo& info) {
  auto& pd = context->flat_plane_geometry_branch_wind_pipeline;
  if (!pd.pipeline_handle.is_valid() || !pd.desc_set0) {
    return;
  }

  //  @TODO: Use a different plane with fewer triangles.
  auto& geom = context->lod0_curved_plane_geometry_buffer;
  auto& inds = context->flat_plane_large_instance_indices;
  auto& agg = context->large_instance_aggregate_buffer;
  if (inds.valid && geom.valid && inds.num_active > 0 && agg.num_active > 0) {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_flat_plane_geometry_branch_wind_forward");
    (void) profiler;

    const auto addtl_dyn_off = uint32_t(
      agg.num_reserved * sizeof(OrnamentalFoliageLargeInstanceAggregateData) * info.frame_index);
    auto pc_data = make_branch_wind_push_constant_data(*context, geom.geometry_params, info);
    draw_grid_geometry(context, pd, geom, inds, &pc_data, info, &addtl_dyn_off);
  }
}

void render_curved_plane_geometry_stem_wind_forward(GPUContext* context, const RenderForwardInfo& info) {
  auto& pd = context->curved_plane_geometry_stem_wind_pipeline;
  if (!pd.pipeline_handle.is_valid() || !pd.desc_set0) {
    return;
  }

  auto& geom = context->lod0_curved_plane_geometry_buffer;
  auto& inds = context->curved_plane_small_instance_indices;
  if (geom.valid && inds.valid && inds.num_active > 0) {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_curved_plane_geometry_stem_wind_forward");
    (void) profiler;

    auto pc_data = make_stem_wind_push_constant_data(*context, geom.geometry_params, info);
    draw_grid_geometry(context, pd, geom, inds, &pc_data, info);
  }
}

void render_curved_plane_geometry_branch_wind_forward(GPUContext* context, const RenderForwardInfo& info) {
  auto& pd = context->curved_plane_geometry_branch_wind_pipeline;
  if (!pd.pipeline_handle.is_valid() || !pd.desc_set0) {
    return;
  }

  auto& geom = context->lod0_curved_plane_geometry_buffer;
  auto& inds = context->curved_plane_large_instance_indices;
  auto& agg = context->large_instance_aggregate_buffer;
  if (geom.valid && inds.valid && inds.num_active > 0 && agg.num_active > 0) {
    auto profiler = GROVE_VK_SCOPED_DEBUG_LABEL(info.cmd, "render_curved_plane_geometry_branch_wind_forward");
    (void) profiler;

    const auto addtl_dyn_off = uint32_t(
      agg.num_reserved * sizeof(OrnamentalFoliageLargeInstanceAggregateData) * info.frame_index);
    auto pc_data = make_branch_wind_push_constant_data(*context, geom.geometry_params, info);
    draw_grid_geometry(context, pd, geom, inds, &pc_data, info, &addtl_dyn_off);
  }
}

void render_forward(GPUContext* context, const RenderForwardInfo& info) {
  if (!context->disabled) {
    render_curved_plane_geometry_stem_wind_forward(context, info);
    render_curved_plane_geometry_branch_wind_forward(context, info);
    render_flat_plane_geometry_stem_wind_forward(context, info);
    render_flat_plane_geometry_branch_wind_forward(context, info);
  }
}

struct {
  GPUContext context;
} globals;

} //  anon

void foliage::render_ornamental_foliage_begin_frame(const BeginFrameInfo& info) {
  begin_frame(&globals.context, info);
}

void foliage::render_ornamental_foliage_render_forward(const RenderForwardInfo& info) {
  render_forward(&globals.context, info);
}

void foliage::terminate_ornamental_foliage_rendering() {
  globals.context = {};
}

RenderOrnamentalFoliageRenderParams* foliage::get_render_ornamental_foliage_render_params() {
  return &globals.context.render_params;
}

RenderOrnamentalFoliageStats foliage::get_render_ornamental_foliage_stats() {
  RenderOrnamentalFoliageStats result{};
  result.num_curved_plane_small_instances = globals.context.curved_plane_small_instance_indices.num_active;
  result.num_curved_plane_large_instances = globals.context.curved_plane_large_instance_indices.num_active;
  result.num_flat_plane_small_instances = globals.context.flat_plane_small_instance_indices.num_active;
  result.num_flat_plane_large_instances = globals.context.flat_plane_large_instance_indices.num_active;
  result.wrote_to_instance_buffers = globals.context.wrote_to_instance_buffers;
  result.wrote_to_indices_buffers = globals.context.wrote_to_indices_buffers;
  return result;
}

void foliage::set_render_ornamental_foliage_wind_displacement_image(uint32_t id) {
  globals.context.wind_image = DynamicSampledImageManager::Handle{id};
}

bool foliage::get_render_ornamental_foliage_disabled() {
  return globals.context.disabled;
}

void foliage::set_render_ornamental_foliage_disabled(bool disable) {
  globals.context.disabled = disable;
}

int foliage::get_render_ornamental_foliage_num_material1_texture_layers() {
  //  @NOTE: This must return the number of layers that *would* be in the image, if it were
  //  created successfully (rather than the number of layers currently in the image, which might
  //  be 0 if it has not been created yet).
  //  create_alpha_test_material_image
  return Config::num_material1_alpha_texture_layers;
}

GROVE_NAMESPACE_END
