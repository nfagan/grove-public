#include "csm.hpp"
#include "grove/visual/Camera.hpp"
#include "grove/common/common.hpp"
#include "grove/math/Bounds3.hpp"
#include "grove/math/matrix_transform.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

float max_extent_distance(const Vec2f& xy_near, const Vec2f& xy_far, float ak, float bk) {
  const float x0 = std::min(xy_near.x, xy_far.x);
  const float y0 = std::min(xy_near.y, xy_far.y);
  const float x1 = std::max(-xy_near.x, -xy_far.x);
  const float y1 = std::max(-xy_near.y, -xy_far.y);

  const Vec3f p0(x0, y0, ak);
  const Vec3f p1(x1, y1, bk);

  float diag_distance = (p1 - p0).length();
  Vec3f p0_far = Vec3f(p0.x, p0.y, p1.z);
  float far_diag_distance = (p1 - p0_far).length();

  return std::ceil(std::max(diag_distance, far_diag_distance));
}

float camera_space_bounding_box_component(float ak, float g, float multiplier) {
  return ak/g * multiplier;
}

Vec2f camera_space_bounding_box_xy(float z, float ar, float g) {
  float x0 = -camera_space_bounding_box_component(z, g, ar);
  float y0 = -camera_space_bounding_box_component(z, g, 1.0f);
  return Vec2f(x0, y0);
}

Vec3f light_space_camera_position(const Bounds3f& from_bounds, float tk) {
  const auto& p0 = from_bounds.min;
  const auto& p1 = from_bounds.max;

  const float x0 = std::floor((p0.x + p1.x) / (2.0f * tk)) * tk;
  const float y0 = std::floor((p0.y + p1.y) / (2.0f * tk)) * tk;

  return Vec3f(x0, y0, p0.z);
}

Bounds3f make_light_space_bounding_box(const Vec2f& xy_near, const Vec2f& xy_far,
                                       float ak, float bk, const Mat4f& cam_to_light) {
  const auto v000 = cam_to_light * Vec4f(xy_near.x, xy_near.y, ak, 1.0f);
  const auto v100 = cam_to_light * Vec4f(-xy_near.x, xy_near.y, ak, 1.0f);
  const auto v110 = cam_to_light * Vec4f(-xy_near.x, -xy_near.y, ak, 1.0f);
  const auto v010 = cam_to_light * Vec4f(xy_near.x, -xy_near.y, ak, 1.0f);

  const auto v001 = cam_to_light * Vec4f(xy_far.x, xy_far.y, bk, 1.0f);
  const auto v101 = cam_to_light * Vec4f(-xy_far.x, xy_far.y, bk, 1.0f);
  const auto v111 = cam_to_light * Vec4f(-xy_far.x, -xy_far.y, bk, 1.0f);
  const auto v011 = cam_to_light * Vec4f(xy_far.x, -xy_far.y, bk, 1.0f);

  const auto min0 = min(min(min(v000, v100), v110), v010);
  const auto min1 = min(min(min(v001, v101), v111), v011);

  const auto max0 = max(max(max(v000, v100), v110), v010);
  const auto max1 = max(max(max(v001, v101), v111), v011);

  const auto p0 = min(min0, min1);
  const auto p1 = max(max0, max1);

  return Bounds3f{to_vec3(p0), to_vec3(p1)};
}

Mat4f make_world_to_light(const Vec3f& sun_position) {
  auto up = Vec3f(0.0f, 1.0f, 0.0f);
  auto world_to_light = look_at(sun_position, Vec3f(0.0f), up);
  world_to_light[3] = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
  return world_to_light;
}

Mat4f make_cam_to_world(const Camera& camera) {
  return inverse(camera.get_view());
}

} //  anon

void csm::update_csm_descriptor(CSMDescriptor& descriptor,
                                const Camera& camera,
                                const Vec3f& sun_position) {
  const auto proj_info = camera.get_projection_info();
  const float ar = proj_info.aspect_ratio;
  const float g = proj_info.projection_plane_distance();

  const auto world_to_light = make_world_to_light(sun_position);
  const auto cam_to_world = make_cam_to_world(camera);
  const auto cam_to_light = world_to_light * cam_to_world;
  descriptor.light_shadow_sample_view = world_to_light;

  for (int i = 0; i < descriptor.num_layers(); i++) {
    const float ak = descriptor.layer_z_offsets[i].x;
    const float bk = descriptor.layer_z_offsets[i].y;

    const auto xy_near = camera_space_bounding_box_xy(ak, ar, g);
    const auto xy_far = camera_space_bounding_box_xy(bk, ar, g);

    const auto light_space_bounds =
      make_light_space_bounding_box(xy_near, xy_far, ak, bk, cam_to_light);

    const float dk = max_extent_distance(xy_near, xy_far, ak, bk);
    //  world space extent per texel-dimension
    const float tk = dk / float(descriptor.texture_size);
    const auto z_diff = light_space_bounds.max.z - light_space_bounds.min.z;

    const auto light_space_camera_pos = light_space_camera_position(light_space_bounds, tk);
    auto world_to_light_k = world_to_light;
    world_to_light_k[3] = Vec4f(-light_space_camera_pos, 1.0f);

    Mat4f light_proj(1.0f);
    light_proj(0, 0) = 2.0f / dk;
    light_proj(1, 1) = 2.0f / dk * descriptor.sign_y;
    light_proj(2, 2) = -1.0f / z_diff;

    descriptor.light_space_view_projections[i] = light_proj * world_to_light_k;

    CSMDescriptor::UVTransform uv_transform{};
    uv_transform.scale = Vec3f{2.0f / dk, 2.0f / dk * descriptor.sign_y, -1.0f / z_diff};
    uv_transform.offset = -light_space_camera_pos;
    descriptor.uv_transforms[i] = uv_transform;
  }
}

csm::CSMDescriptor csm::make_csm_descriptor(int num_layers,
                                            int texture_size,
                                            float layer_size,
                                            float layer_increment,
                                            float sign_y) {
  CSMDescriptor result{};
  result.layer_z_offsets.resize(num_layers);
  result.light_space_view_projections.resize(num_layers);
  result.uv_transforms.resize(num_layers);
  result.texture_size = texture_size;
  result.layer_size = layer_size;
  result.layer_increment = layer_increment;
  result.sign_y = sign_y;
  //  Make offsets.
  float offset = 0.0f;
  for (int i = 0; i < num_layers; i++) {
    const float ak = offset;
    const float bk = ak + layer_size;
    result.layer_z_offsets[i] = Vec2f(ak, bk);
    offset += layer_increment;
  }
  return result;
}

csm::CSMDescriptor csm::make_csm_descriptor(int num_layers,
                                            int texture_size,
                                            const float* layer_sizes,
                                            float sign_y) {
  CSMDescriptor result{};
  result.layer_z_offsets.resize(num_layers);
  result.light_space_view_projections.resize(num_layers);
  result.uv_transforms.resize(num_layers);
  result.texture_size = texture_size;
  result.layer_size = layer_sizes[0];
  result.layer_increment = layer_sizes[0];
  result.sign_y = sign_y;
  //  Make offsets.
  float offset = 0.0f;
  for (int i = 0; i < num_layers; i++) {
    result.layer_z_offsets[i] = Vec2f(offset, offset + layer_sizes[i]);
    offset += layer_sizes[i];
  }
  return result;
}

GROVE_NAMESPACE_END
