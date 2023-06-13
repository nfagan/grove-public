#include "distribute_along_axis.hpp"
#include "grove/math/matrix_transform.hpp"
#include "grove/math/frame.hpp"
#include "grove/math/random.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace geometry;

using BufferIndices = DistributeAlongAxisBufferIndices;
using BufferDesc = VertexBufferDescriptor;

size_t invalid_offset() {
  return size_t(~uint64_t(0));
}

void set_offsets(const BufferDesc& desc, const BufferIndices& indices, size_t* dst) {
  dst[0] = desc.ith_attribute_offset_bytes(indices.pos_attr); //  not optional
  if (indices.norm_attr) {
    dst[1] = desc.ith_attribute_offset_bytes(indices.norm_attr.value());
  } else {
    dst[1] = invalid_offset();
  }
  if (indices.uv_attr) {
    dst[2] = desc.ith_attribute_offset_bytes(indices.uv_attr.value());
  } else {
    dst[2] = invalid_offset();
  }
}

[[maybe_unused]] void validate(const BufferDesc& desc, const BufferIndices& inds) {
  const auto is_float3 = [&](const AttributeDescriptor& desc) { return desc.is_floatn(3); };
  const auto is_float2 = [&](const AttributeDescriptor& desc) { return desc.is_floatn(2); };
  const auto& attrs = desc.get_attributes();
  assert(is_float3(attrs[inds.pos_attr]));
  if (inds.norm_attr) {
    assert(is_float3(attrs[inds.norm_attr.value()]));
  }
  if (inds.uv_attr) {
    assert(is_float2(attrs[inds.uv_attr.value()]));
  }
  (void) desc;
  (void) inds;
  (void) is_float3;
  (void) is_float2;
  (void) attrs;
}

[[maybe_unused]] void validate(const BufferIndices& in, const BufferIndices& out) {
  assert(bool(in.norm_attr) == bool(out.norm_attr));
  assert(bool(in.uv_attr) == bool(out.uv_attr));
  (void) in;
  (void) out;
}

void make_coordinate_system(Vec3f* ai, Vec3f* aj, Vec3f* ak,
                            const DistributeAlongAxisParams& params) {
  const float step_axis_len = params.step_axis.length();
  if (step_axis_len == 0.0f) {
    *ai = ConstVec3f::positive_x;
    *aj = ConstVec3f::positive_y;
    *ak = ConstVec3f::positive_z;
  } else {
    make_coordinate_system_y(normalize(params.step_axis), ai, aj, ak);
  }
}

} //  anon

size_t geometry::distribute_along_axis(const void* in, const BufferDesc& in_desc,
                                       size_t in_size, const BufferIndices& in_indices,
                                       void* out, const BufferDesc& out_desc,
                                       size_t max_out_size, const BufferIndices& out_indices,
                                       const DistributeAlongAxisParams& params) {
#ifdef GROVE_DEBUG
  validate(in_desc, in_indices);
  validate(out_desc, out_indices);
  validate(in_indices, out_indices);
#endif

  const size_t src_verts = in_desc.num_vertices(in_size);
  const size_t max_dst_verts = out_desc.num_vertices(max_out_size);

  const size_t src_stride = in_desc.attribute_stride_bytes();
  size_t src_offs[3];
  set_offsets(in_desc, in_indices, src_offs);

  const size_t dst_stride = out_desc.attribute_stride_bytes();
  size_t dst_offs[3];
  set_offsets(out_desc, out_indices, dst_offs);

  const auto* src_char = static_cast<const unsigned char*>(in);
  auto* dst_char = static_cast<unsigned char*>(out);

  Vec3f ai;
  Vec3f aj;
  Vec3f ak;
  make_coordinate_system(&ai, &aj, &ak, params);
  const auto base_axis_off = ai * params.base_axis_offset.x +
                             aj * params.base_axis_offset.y +
                             ak * params.base_axis_offset.z;

  size_t dst_vi{};
  for (int i = 0; i < params.num_steps; i++) {
    const DistributeAlongAxisStep step = params.step(i);
    auto axis_off = aj * float(i) * params.step_length;

    for (int inst = 0; inst < step.num_instances; inst++) {
      const auto& th_lims = step.theta_randomness_limits;
      const float th_rand_scale = urandf() * (th_lims.y - th_lims.x) + th_lims.x;
      float theta = (float(inst) / float(step.num_instances)) * float(two_pi());
      theta += th_rand_scale;

      auto rot_off = Vec3f{std::cos(theta), 0.0f, -std::sin(theta)};
      rot_off = step.radius * (ai * rot_off.x + ak * rot_off.z);

      const auto& sr_lims = step.scale_randomness_limits;
      const float sr_rand_scale = urandf() * (sr_lims.y - sr_lims.x) + sr_lims.x;
      Vec3f step_scale = step.scale + sr_rand_scale;

      auto thetas = step.max_rotation * Vec3f{urandf(), urandf(), urandf()};
      auto rot_m = make_x_rotation(thetas.x) *
                   make_y_rotation(thetas.y) *
                   make_z_rotation(thetas.z);

      for (size_t v = 0; v < src_verts; v++) {
        Vec3f p;
        memcpy(&p, src_char + v * src_stride + src_offs[0], sizeof(Vec3f));

        Vec3f n{};
        if (src_offs[1] != invalid_offset()) {
          memcpy(&n, src_char + v * src_stride + src_offs[1], sizeof(Vec3f));
        }

        Vec2f uv{};
        if (src_offs[2] != invalid_offset()) {
          memcpy(&uv, src_char + v * src_stride + src_offs[2], sizeof(Vec2f));
        }

        p = (step_scale * to_vec3(rot_m * Vec4f{p, 0.0f})) + base_axis_off + rot_off + axis_off;
        n = to_vec3(rot_m * Vec4f{n, 0.0f});

        if (dst_vi >= max_dst_verts) {
          //  out of bounds.
          return dst_vi;
        }

        size_t dst_off_p = dst_vi * dst_stride + dst_offs[0];
        memcpy(dst_char + dst_off_p, &p, sizeof(Vec3f));
        if (dst_offs[1] != invalid_offset()) {
          size_t dst_off_n = dst_vi * dst_stride + dst_offs[1];
          memcpy(dst_char + dst_off_n, &n, sizeof(Vec3f));
        }
        if (dst_offs[2] != invalid_offset()) {
          size_t dst_off_uv = dst_vi * dst_stride + dst_offs[2];
          memcpy(dst_char + dst_off_uv, &uv, sizeof(Vec2f));
        }
        dst_vi++;
      }
    }
  }

  return dst_vi;
}

GROVE_NAMESPACE_END